#include "ti_msp_dl_config.h"

#include "config/gimbal_tracking_config.h"
#include "pitch_motor_control.h"
#include "pitch_tracker_control.h"
#include "target_recovery_control.h"
#include "vision_config.h"
#include "vision_uart.h"

#include <stdio.h>

/* PB6/PB7 controls yaw; PA23/PA24 controls pitch. */
static pitch_motor_t g_pitch_motor;
static pitch_motor_t g_yaw_motor;
static pitch_motor_commissioning_t g_yaw_commissioning;
static pitch_motor_commissioning_t g_pitch_commissioning;
static pitch_tracker_control_t g_yaw_tracker;
static pitch_tracker_control_t g_pitch_tracker;
static target_recovery_control_t g_target_recovery;
static volatile uint32_t g_monotonic_ms;
static bool g_vision_uart_ready;

typedef enum {
    PD_TUNING_WAIT_CENTER = 0U,
    PD_TUNING_PERTURB,
    PD_TUNING_RELEASE,
    PD_TUNING_COOLDOWN,
    PD_TUNING_COMPLETE,
    PD_TUNING_ABORTED,
} pd_tuning_state_t;

typedef struct {
    pd_tuning_state_t state;
    uint8_t trial_index;
    uint32_t state_started_ms;
    uint32_t stable_started_ms;
    uint32_t last_perturb_ms;
    bool yaw_abort_stop_submitted;
    bool pitch_abort_stop_submitted;
} pd_tuning_context_t;

static pd_tuning_context_t g_pd_tuning;

static const vision_uart_config_t g_maixcam_uart = {
    .instance = maixcam_INST,
    .irqn = maixcam_INST_INT_IRQN,
    .frame_width = VISION_DEFAULT_FRAME_WIDTH,
    .frame_height = VISION_DEFAULT_FRAME_HEIGHT,
};

static const target_recovery_config_t g_target_recovery_config = {
    .prediction_hold_ms = TARGET_LOSS_PREDICTION_HOLD_MS,
    .scan_command_period_ms = TARGET_SEARCH_SCAN_PERIOD_MS,
    .yaw_scan_half_span_pulses = TARGET_SEARCH_YAW_HALF_SPAN_PULSES,
    .pitch_scan_half_span_pulses = TARGET_SEARCH_PITCH_HALF_SPAN_PULSES,
    .yaw_scan_step_pulses = TARGET_SEARCH_YAW_STEP_PULSES,
    .pitch_scan_step_pulses = TARGET_SEARCH_PITCH_STEP_PULSES,
};

void SysTick_Handler(void)
{
    g_monotonic_ms++;
}

void UART0_IRQHandler(void)
{
    vision_uart_on_uart_irq();
}

static bool error_is_centered(int32_t error, int16_t deadband)
{
    return (error <= deadband) && (error >= -deadband);
}

static void pd_tuning_transition(pd_tuning_state_t state, uint32_t now_ms)
{
    g_pd_tuning.state = state;
    g_pd_tuning.state_started_ms = now_ms;
    g_pd_tuning.stable_started_ms = 0U;
    if (state == PD_TUNING_ABORTED) {
        g_pd_tuning.yaw_abort_stop_submitted = false;
        g_pd_tuning.pitch_abort_stop_submitted = false;
    }
}

static bool pd_tuning_trial_is_yaw(void)
{
    return g_pd_tuning.trial_index < 2U;
}

static int32_t pd_tuning_test_error(int32_t dx, int32_t dy)
{
    return pd_tuning_trial_is_yaw() ? dx : dy;
}

static int32_t pd_tuning_desired_error_sign(void)
{
    return ((g_pd_tuning.trial_index & 1U) == 0U) ? 1 : -1;
}

static pitch_motor_t *pd_tuning_test_motor(void)
{
    return pd_tuning_trial_is_yaw() ? &g_yaw_motor : &g_pitch_motor;
}

static pitch_tracker_control_t *pd_tuning_test_tracker(void)
{
    return pd_tuning_trial_is_yaw() ? &g_yaw_tracker : &g_pitch_tracker;
}

static int32_t pd_tuning_target_error_magnitude(void)
{
    return pd_tuning_trial_is_yaw() ?
        PD_TUNING_YAW_TARGET_ERROR_PIXELS :
        PD_TUNING_PITCH_TARGET_ERROR_PIXELS;
}

static int32_t pd_tuning_perturb_pulses(void)
{
    const pitch_tracker_control_t *tracker = pd_tuning_test_tracker();
    int32_t correction_sign = tracker->positive_error_is_cw ? 1 : -1;

    return -pd_tuning_desired_error_sign() * correction_sign *
        PD_TUNING_PERTURB_STEP_PULSES;
}

static void pd_tuning_run_trackers(bool target_valid, int16_t dx, int16_t dy,
                                   uint32_t now_ms, bool *pitch_sent,
                                   bool *yaw_sent)
{
    *pitch_sent = pitch_tracker_update(&g_pitch_tracker, target_valid, dy,
                                       now_ms);
    *yaw_sent = pitch_tracker_update(&g_yaw_tracker, target_valid, dx, now_ms);
}

static void pd_tuning_update(bool target_valid, int16_t dx, int16_t dy,
                             uint32_t now_ms, bool *pitch_sent,
                             bool *yaw_sent)
{
    bool centered = target_valid &&
        error_is_centered(dx, g_yaw_tracker.deadband_pixels) &&
        error_is_centered(dy, g_pitch_tracker.deadband_pixels);
    int32_t test_error = pd_tuning_test_error(dx, dy);

    *pitch_sent = false;
    *yaw_sent = false;

    switch (g_pd_tuning.state) {
    case PD_TUNING_WAIT_CENTER:
        pd_tuning_run_trackers(target_valid, dx, dy, now_ms, pitch_sent,
                               yaw_sent);
        if (!centered) {
            g_pd_tuning.stable_started_ms = 0U;
        } else if (g_pd_tuning.stable_started_ms == 0U) {
            g_pd_tuning.stable_started_ms = now_ms;
        } else if ((uint32_t) (now_ms - g_pd_tuning.stable_started_ms) >=
                   PD_TUNING_CENTER_STABLE_MS) {
            pd_tuning_transition(PD_TUNING_PERTURB, now_ms);
            g_pd_tuning.last_perturb_ms = now_ms -
                PD_TUNING_PERTURB_PERIOD_MS;
        }
        break;

    case PD_TUNING_PERTURB:
        if (!target_valid ||
            ((uint32_t) (now_ms - g_pd_tuning.state_started_ms) >=
             PD_TUNING_PERTURB_TIMEOUT_MS)) {
            (void) pitch_motor_stop(pd_tuning_test_motor());
            pd_tuning_transition(PD_TUNING_ABORTED, now_ms);
            break;
        }

        if (pd_tuning_trial_is_yaw()) {
            *pitch_sent = pitch_tracker_update(&g_pitch_tracker, true, dy,
                                               now_ms);
        } else {
            *yaw_sent = pitch_tracker_update(&g_yaw_tracker, true, dx,
                                             now_ms);
        }

        if ((pd_tuning_desired_error_sign() * test_error) >=
            pd_tuning_target_error_magnitude()) {
            if (pitch_motor_stop(pd_tuning_test_motor())) {
                pitch_tracker_prepare_response(pd_tuning_test_tracker(),
                                               (int16_t) test_error, now_ms);
                pd_tuning_transition(PD_TUNING_RELEASE, now_ms);
            }
        } else if ((uint32_t) (now_ms - g_pd_tuning.last_perturb_ms) >=
                   PD_TUNING_PERTURB_PERIOD_MS) {
            if (pitch_motor_move_relative(
                    pd_tuning_test_motor(), pd_tuning_perturb_pulses(),
                    PD_TUNING_PERTURB_SPEED_RPM,
                    PD_TUNING_PERTURB_ACCELERATION)) {
                g_pd_tuning.last_perturb_ms = now_ms;
            }
        }
        break;

    case PD_TUNING_RELEASE:
        pd_tuning_run_trackers(target_valid, dx, dy, now_ms, pitch_sent,
                               yaw_sent);
        if (!target_valid ||
            ((uint32_t) (now_ms - g_pd_tuning.state_started_ms) >=
             PD_TUNING_RESPONSE_TIMEOUT_MS)) {
            pd_tuning_transition(PD_TUNING_ABORTED, now_ms);
        } else if (!centered) {
            g_pd_tuning.stable_started_ms = 0U;
        } else if (g_pd_tuning.stable_started_ms == 0U) {
            g_pd_tuning.stable_started_ms = now_ms;
        } else if ((uint32_t) (now_ms - g_pd_tuning.stable_started_ms) >=
                   PD_TUNING_SETTLED_MS) {
            pd_tuning_transition(PD_TUNING_COOLDOWN, now_ms);
        }
        break;

    case PD_TUNING_COOLDOWN:
        pd_tuning_run_trackers(target_valid, dx, dy, now_ms, pitch_sent,
                               yaw_sent);
        if ((uint32_t) (now_ms - g_pd_tuning.state_started_ms) >=
            PD_TUNING_COOLDOWN_MS) {
            g_pd_tuning.trial_index++;
            pd_tuning_transition(
                (g_pd_tuning.trial_index >= PD_TUNING_TRIAL_COUNT) ?
                    PD_TUNING_COMPLETE : PD_TUNING_WAIT_CENTER,
                now_ms);
        }
        break;

    case PD_TUNING_COMPLETE:
        pd_tuning_run_trackers(target_valid, dx, dy, now_ms, pitch_sent,
                               yaw_sent);
        break;

    case PD_TUNING_ABORTED:
    default:
        if (!g_pd_tuning.yaw_abort_stop_submitted) {
            g_pd_tuning.yaw_abort_stop_submitted =
                pitch_motor_stop(&g_yaw_motor);
        }
        if (!g_pd_tuning.pitch_abort_stop_submitted) {
            g_pd_tuning.pitch_abort_stop_submitted =
                pitch_motor_stop(&g_pitch_motor);
        }
        break;
    }
}

static void handle_vision_observation(const vision_observation_t *observation)
{
    char line[96];
    int32_t dx;
    int32_t dy;
    pitch_tracker_diagnostics_t pitch_diagnostics;
    pitch_tracker_diagnostics_t yaw_diagnostics;
    target_recovery_output_t recovery_output;
    bool pitch_command_sent = false;
    bool yaw_command_sent = false;

    if (observation == NULL) {
        return;
    }

    dx = (int32_t) observation->target_x -
         ((int32_t) observation->frame_width / 2);
    dy = (int32_t) observation->target_y -
         ((int32_t) observation->frame_height / 2);
#if GIMBAL_PD_TUNING_ENABLED
    pd_tuning_update(observation->target_valid, (int16_t) dx, (int16_t) dy,
                     g_monotonic_ms, &pitch_command_sent,
                     &yaw_command_sent);
#else
    target_recovery_control_update(
        &g_target_recovery, observation->target_valid,
        (int16_t) dx, (int16_t) dy, g_monotonic_ms, &recovery_output);

    if (recovery_output.stop_trackers) {
        (void) pitch_tracker_update(&g_pitch_tracker, false, 0,
                                    g_monotonic_ms);
        (void) pitch_tracker_update(&g_yaw_tracker, false, 0,
                                    g_monotonic_ms);
    }
    if (recovery_output.reset_trackers) {
        pitch_tracker_prepare_response(
            &g_pitch_tracker, recovery_output.tracking_dy, g_monotonic_ms);
        pitch_tracker_prepare_response(
            &g_yaw_tracker, recovery_output.tracking_dx, g_monotonic_ms);
    }

    if (recovery_output.use_tracker) {
#if PITCH_TRACKING_ENABLED
        pitch_command_sent = pitch_tracker_update(
            &g_pitch_tracker, true, recovery_output.tracking_dy,
            g_monotonic_ms);
#endif
#if YAW_TRACKING_ENABLED
        yaw_command_sent = pitch_tracker_update(
            &g_yaw_tracker, true, recovery_output.tracking_dx,
            g_monotonic_ms);
#endif
    } else if (recovery_output.mode == TARGET_RECOVERY_SCANNING) {
#if YAW_TRACKING_ENABLED
        if (recovery_output.yaw_scan_pulses != 0) {
            yaw_command_sent = pitch_motor_move_relative(
                &g_yaw_motor, recovery_output.yaw_scan_pulses,
                TARGET_SEARCH_SPEED_RPM, TARGET_SEARCH_ACCELERATION);
        }
#endif
#if PITCH_TRACKING_ENABLED
        if (recovery_output.pitch_scan_pulses != 0) {
            pitch_command_sent = pitch_motor_move_relative(
                &g_pitch_motor, recovery_output.pitch_scan_pulses,
                TARGET_SEARCH_SPEED_RPM, TARGET_SEARCH_ACCELERATION);
        }
#endif
        target_recovery_commit_scan(
            &g_target_recovery, recovery_output.yaw_scan_pulses,
            yaw_command_sent, recovery_output.pitch_scan_pulses,
            pitch_command_sent, g_monotonic_ms);
    }
#endif
#if GIMBAL_PD_TUNING_ENABLED
    pitch_tracker_get_diagnostics(&g_pitch_tracker, &pitch_diagnostics);
    pitch_tracker_get_diagnostics(&g_yaw_tracker, &yaw_diagnostics);
    (void) snprintf(
        line, sizeof(line),
        "PD,%lu,%u,%u,%u,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld",
        (unsigned long) g_monotonic_ms, (unsigned int) g_pd_tuning.state,
        (unsigned int) g_pd_tuning.trial_index,
        observation->target_valid ? 1U : 0U, (long) dx, (long) dy,
        (long) yaw_diagnostics.p_term_milli_pulses,
        (long) yaw_diagnostics.d_term_milli_pulses,
        (long) yaw_diagnostics.output_pulses,
        (long) pitch_diagnostics.p_term_milli_pulses,
        (long) pitch_diagnostics.d_term_milli_pulses,
        (long) pitch_diagnostics.output_pulses);
#else
    (void) snprintf(line, sizeof(line),
                    "TV,%u,%ld,%ld,%u,%u,CS,%u,%u,RM,%u",
                    observation->target_valid ? 1U : 0U,
                    (long) dx, (long) dy,
                    observation->target_x, observation->target_y,
                    pitch_command_sent ? 1U : 0U,
                    yaw_command_sent ? 1U : 0U,
                    (unsigned int) recovery_output.mode);
#endif
    (void) vision_uart_send_line(line);
}

static void process_latest_maixcam_observation(void)
{
    vision_observation_t observation;

    if (vision_uart_take_latest_observation(&observation)) {
        handle_vision_observation(&observation);
    }
}

static void motor_diagnostic_update(uint32_t now_ms)
{
    static uint32_t last_report_ms;
    static bool report_pitch;
    pitch_motor_diagnostics_t diagnostics;
    const pitch_motor_t *motor;
    char axis;
    char line[96];

    if ((uint32_t) (now_ms - last_report_ms) <
        MOTOR_DIAGNOSTIC_PERIOD_MS) {
        return;
    }

    motor = report_pitch ? &g_pitch_motor : &g_yaw_motor;
    axis = report_pitch ? 'P' : 'Y';
    pitch_motor_get_diagnostics(motor, &diagnostics);
    (void) snprintf(
        line, sizeof(line), "MS,%c,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
        axis,
        (unsigned long) diagnostics.command_queued_count,
        (unsigned long) diagnostics.command_rejected_count,
        (unsigned long) diagnostics.tx_frame_completed_count,
        (unsigned long) diagnostics.tx_timeout_count,
        (unsigned long) diagnostics.response_accepted_count,
        (unsigned long) diagnostics.response_reached_count,
        (unsigned long) diagnostics.response_protection_count,
        (unsigned long) diagnostics.response_protocol_error_count);
    if (vision_uart_send_line(line)) {
        last_report_ms = now_ms;
        report_pitch = !report_pitch;
    }
}

static void led_heartbeat_update(uint32_t now_ms)
{
    static uint32_t last_toggle_ms;

    if ((uint32_t)(now_ms - last_toggle_ms) < LED_HEARTBEAT_PERIOD_MS) {
        return;
    }

    last_toggle_ms = now_ms;
    DL_GPIO_togglePins(GPIO_GRP_0_PORT, GPIO_GRP_0_LED_1_PIN);
}

static bool dual_motor_commissioning_is_complete(void)
{
    return (g_yaw_commissioning.state ==
            PITCH_MOTOR_COMMISSIONING_COMPLETE) &&
           (g_pitch_commissioning.state ==
            PITCH_MOTOR_COMMISSIONING_COMPLETE);
}

static void configure_tracking_commissioning_mode(void)
{
#if GIMBAL_TRACKING_COMMISSIONING_MODE
    g_yaw_tracker.deadband_pixels = GIMBAL_COMMISSIONING_DEADBAND_PIXELS;
    g_yaw_tracker.kp_pulses_per_pixel = GIMBAL_YAW_KP_PULSES_PER_PIXEL;
    g_yaw_tracker.kd_pulse_seconds_per_pixel =
        GIMBAL_YAW_KD_PULSE_SECONDS_PER_PIXEL;
    g_yaw_tracker.maximum_pulses = GIMBAL_YAW_COMMISSIONING_MAXIMUM_PULSES;
    g_yaw_tracker.command_period_ms = GIMBAL_CONTROL_PERIOD_MS;

    g_pitch_tracker.deadband_pixels = GIMBAL_COMMISSIONING_DEADBAND_PIXELS;
    g_pitch_tracker.kp_pulses_per_pixel = GIMBAL_PITCH_KP_PULSES_PER_PIXEL;
    g_pitch_tracker.kd_pulse_seconds_per_pixel =
        GIMBAL_PITCH_KD_PULSE_SECONDS_PER_PIXEL;
    g_pitch_tracker.maximum_pulses =
        GIMBAL_PITCH_COMMISSIONING_MAXIMUM_PULSES;
    g_pitch_tracker.command_period_ms = GIMBAL_CONTROL_PERIOD_MS;
#endif
}

int main(void)
{
    SYSCFG_DL_init();
    (void) SysTick_Config(CPUCLK_FREQ / 1000U);
    pitch_motor_init(&g_yaw_motor, stepMotor1_INST, 1U);
    pitch_motor_init(&g_pitch_motor, stepMotor2_INST, 1U);
    (void) pitch_motor_enable(&g_pitch_motor);
    (void) pitch_motor_enable(&g_yaw_motor);
    pitch_tracker_control_init(&g_yaw_tracker, &g_yaw_motor);
    pitch_tracker_control_init(&g_pitch_tracker, &g_pitch_motor);
    g_yaw_tracker.positive_error_is_cw = GIMBAL_YAW_POSITIVE_ERROR_IS_CW;
    g_pitch_tracker.positive_error_is_cw = GIMBAL_PITCH_POSITIVE_ERROR_IS_CW;
    configure_tracking_commissioning_mode();
    target_recovery_control_init(&g_target_recovery,
                                 &g_target_recovery_config);

#if PITCH_MOTOR_COMMISSIONING_TEST_ENABLED
    pitch_motor_commissioning_start(&g_yaw_commissioning, &g_yaw_motor,
                                    g_monotonic_ms);
    pitch_motor_commissioning_start(&g_pitch_commissioning, &g_pitch_motor,
                                    g_monotonic_ms);
#endif

    while (1) {
        led_heartbeat_update(g_monotonic_ms);
#if PITCH_MOTOR_COMMISSIONING_TEST_ENABLED
        pitch_motor_commissioning_update(&g_yaw_commissioning,
                                         g_monotonic_ms);
        pitch_motor_commissioning_update(&g_pitch_commissioning,
                                         g_monotonic_ms);

        if ((!g_vision_uart_ready) && dual_motor_commissioning_is_complete()) {
            g_vision_uart_ready = vision_uart_init(&g_maixcam_uart);
            if (g_vision_uart_ready) {
                (void) vision_uart_send_line("VISION_READY");
            }
        }
#else
        if (!g_vision_uart_ready) {
            g_vision_uart_ready = vision_uart_init(&g_maixcam_uart);
        }
#endif

        if (g_vision_uart_ready) {
            (void) vision_uart_process(g_monotonic_ms);
            process_latest_maixcam_observation();
#if MOTOR_DIAGNOSTIC_ENABLED
            motor_diagnostic_update(g_monotonic_ms);
#endif
        }
        pitch_motor_poll(&g_pitch_motor);
        pitch_motor_poll(&g_yaw_motor);
        pitch_motor_service_tx(&g_yaw_motor, g_monotonic_ms);
        pitch_motor_service_tx(&g_pitch_motor, g_monotonic_ms);
        if (g_vision_uart_ready) {
            vision_uart_service_tx(g_monotonic_ms);
        }
    }
}
