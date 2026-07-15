#include "ti_msp_dl_config.h"

#include "pitch_motor_control.h"
#include "pitch_tracker_control.h"
#include "vision_uart.h"

#include <stdio.h>

#define PITCH_MOTOR_COMMISSIONING_TEST_ENABLED (1U)
#define YAW_TRACKING_ENABLED                   (1U)
#define PITCH_TRACKING_ENABLED                 (1U)
#define GIMBAL_TRACKING_COMMISSIONING_MODE      (1U)
#define LED_HEARTBEAT_PERIOD_MS                  (500U)
#define GIMBAL_YAW_POSITIVE_ERROR_IS_CW         (false)
#define GIMBAL_PITCH_POSITIVE_ERROR_IS_CW       (true)
#define GIMBAL_COMMISSIONING_DEADBAND_PIXELS     (4)
#define GIMBAL_COMMISSIONING_PULSES_PER_PIXEL   (8.0f)
#define GIMBAL_YAW_COMMISSIONING_MAXIMUM_PULSES   (400)
#define GIMBAL_PITCH_COMMISSIONING_MAXIMUM_PULSES (400)
#define MOTOR_DIAGNOSTIC_PERIOD_MS               (500U)
#define MOTOR_DIAGNOSTIC_ENABLED                    (1U)

/* PB6/PB7 controls yaw; PA23/PA24 controls pitch. */
static pitch_motor_t g_pitch_motor;
static pitch_motor_t g_yaw_motor;
static pitch_motor_commissioning_t g_yaw_commissioning;
static pitch_motor_commissioning_t g_pitch_commissioning;
static pitch_tracker_control_t g_yaw_tracker;
static pitch_tracker_control_t g_pitch_tracker;
static volatile uint32_t g_monotonic_ms;
static bool g_vision_uart_ready;

static const vision_uart_config_t g_maixcam_uart = {
    .instance = maixcam_INST,
    .irqn = maixcam_INST_INT_IRQN,
    .frame_width = 512U,
    .frame_height = 320U,
};

void SysTick_Handler(void)
{
    g_monotonic_ms++;
}

void UART0_IRQHandler(void)
{
    vision_uart_on_uart_irq();
}

static void handle_vision_observation(const vision_observation_t *observation)
{
    char line[72];
    int32_t dx;
    int32_t dy;
    bool pitch_command_sent = false;
    bool yaw_command_sent = false;

    if (observation == NULL) {
        return;
    }

    dx = (int32_t) observation->target_x -
         ((int32_t) observation->frame_width / 2);
    dy = (int32_t) observation->target_y -
         ((int32_t) observation->frame_height / 2);
#if PITCH_TRACKING_ENABLED
    pitch_command_sent = pitch_tracker_update(
        &g_pitch_tracker, observation->target_valid, (int16_t) dy,
        g_monotonic_ms);
#endif
#if YAW_TRACKING_ENABLED
    yaw_command_sent = pitch_tracker_update(
        &g_yaw_tracker, observation->target_valid, (int16_t) dx,
        g_monotonic_ms);
#endif
    (void) snprintf(line, sizeof(line), "TV,%u,%ld,%ld,%u,%u,CS,%u,%u",
                    observation->target_valid ? 1U : 0U,
                    (long) dx, (long) dy,
                    observation->target_x, observation->target_y,
                    pitch_command_sent ? 1U : 0U,
                    yaw_command_sent ? 1U : 0U);
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
    g_yaw_tracker.pulses_per_pixel = GIMBAL_COMMISSIONING_PULSES_PER_PIXEL;
    g_yaw_tracker.maximum_pulses = GIMBAL_YAW_COMMISSIONING_MAXIMUM_PULSES;

    g_pitch_tracker.deadband_pixels = GIMBAL_COMMISSIONING_DEADBAND_PIXELS;
    g_pitch_tracker.pulses_per_pixel = GIMBAL_COMMISSIONING_PULSES_PER_PIXEL;
    g_pitch_tracker.maximum_pulses =
        GIMBAL_PITCH_COMMISSIONING_MAXIMUM_PULSES;
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
