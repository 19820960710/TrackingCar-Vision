#include "app/gimbal_autotune.h"

#include "FreeRTOS.h"
#include "task.h"
#include "app/vision_tracking.h"
#include "config/gimbal_autotune_config.h"
#include "config/vision_tracking_config.h"
#include "service/stepper_service.h"
#include "zdt_x42s/zdt_x42s.h"

#include <stdbool.h>
#include <stddef.h>

#define GIMBAL_AUTOTUNE_TRIAL_OK             0U
#define GIMBAL_AUTOTUNE_TRIAL_OFFSET_TIMEOUT 2U
#define GIMBAL_AUTOTUNE_TRIAL_TRACK_TIMEOUT  3U
#define GIMBAL_AUTOTUNE_TRIAL_ABORTED        4U
#define GIMBAL_AUTOTUNE_TRIAL_STATE_ERROR   10U
#define GIMBAL_AUTOTUNE_TRIAL_MOVE_ERROR    12U
#define GIMBAL_AUTOTUNE_TRIAL_POSITION_ERROR 13U

volatile gimbal_autotune_mailbox_t g_gimbal_autotune = {0};

typedef struct {
    uint32_t stable_count;
    uint32_t settle_ms;
    uint32_t stable_error_sum;
    uint32_t zero_crossings;
    uint32_t max_pulse_step;
    int32_t previous_error_sign;
    int32_t previous_pulses;
    bool have_previous_pulses;
} axis_metrics_t;

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static uint32_t random_next(uint32_t *state)
{
    *state = *state * 1664525U + 1013904223U;
    return *state;
}

static uint32_t abs_i32(int32_t value)
{
    return (uint32_t)(value < 0 ? -value : value);
}

static int32_t error_sign_outside_deadband(int32_t error)
{
    if (error > VISION_TRACKING_DEADBAND_PIXELS) {
        return 1;
    }
    if (error < -VISION_TRACKING_DEADBAND_PIXELS) {
        return -1;
    }
    return 0;
}

static int32_t next_offset(uint32_t *state, uint32_t maximum)
{
    uint32_t magnitude = GIMBAL_AUTOTUNE_OFFSET_MIN_PULSES +
        random_next(state) %
        (maximum - GIMBAL_AUTOTUNE_OFFSET_MIN_PULSES + 1U);
    /* An LCG's lowest bit only alternates. Because each offset consumes two
     * values, using bit 0 made every trial for a seed choose one direction. */
    return (random_next(state) & (1UL << 16U)) ? (int32_t)magnitude :
                                                -(int32_t)magnitude;
}

static bool move_axis(stepper_axis_t axis, int32_t pulses)
{
    stepper_motor_move_t move = {
        .direction = (pulses >= 0) ? ZDT_X42S_DIRECTION_CW :
                                     ZDT_X42S_DIRECTION_CCW,
        .speed_rpm = GIMBAL_AUTOTUNE_SPEED_RPM,
        .acceleration = GIMBAL_AUTOTUNE_ACCELERATION,
        .pulse_count = abs_i32(pulses),
        /* A commissioning offset is one bounded move from the settled target.
         * Mode 0 matches the validated FreeRTOS integration. Mode 2 remains
         * reserved for the visual loop's stream of small corrections. */
        .motion_mode = ZDT_X42S_MOTION_RELATIVE_TARGET,
        .sync_flag = 0U,
    };
    return stepper_service_move_axis(axis, &move);
}

static void snapshot_begin(void)
{
    g_gimbal_autotune.snapshot_seq++;
}

static void snapshot_end(void)
{
    g_gimbal_autotune.snapshot_seq++;
}

static void publish_observation(void)
{
    snapshot_begin();
    g_gimbal_autotune.observed_target_valid =
        g_vision_tracking_debug.last_target_valid;
    g_gimbal_autotune.observed_dx =
        g_vision_tracking_debug.last_error_x_pixels;
    g_gimbal_autotune.observed_dy =
        g_vision_tracking_debug.last_error_y_pixels;
    snapshot_end();
}

static bool command_valid(const gimbal_autotune_command_t *command)
{
    return command->request_seq != 0U &&
           command->request_seq_inv == ~command->request_seq &&
           command->trial_count != 0U &&
           command->trial_count <= GIMBAL_AUTOTUNE_MAX_TRIALS &&
           (command->yaw_max_pulses == 0U ||
            (command->yaw_max_pulses >= GIMBAL_AUTOTUNE_OFFSET_MIN_PULSES &&
             command->yaw_max_pulses <=
                 GIMBAL_AUTOTUNE_OFFSET_MAX_YAW_PULSES)) &&
           (command->pitch_max_pulses == 0U ||
            (command->pitch_max_pulses >= GIMBAL_AUTOTUNE_OFFSET_MIN_PULSES &&
             command->pitch_max_pulses <=
                 GIMBAL_AUTOTUNE_OFFSET_MAX_PITCH_PULSES)) &&
           (command->yaw_max_pulses != 0U ||
            command->pitch_max_pulses != 0U) &&
           command->yaw_kp >= 0.0f &&
           command->yaw_kp <= GIMBAL_AUTOTUNE_YAW_KP_MAX &&
           command->yaw_kd >= 0.0f &&
           command->yaw_kd <= GIMBAL_AUTOTUNE_YAW_KD_MAX &&
           command->pitch_kp >= 0.0f &&
           command->pitch_kp <= GIMBAL_AUTOTUNE_PITCH_KP_MAX &&
           command->pitch_kd >= 0.0f &&
           command->pitch_kd <= GIMBAL_AUTOTUNE_PITCH_KD_MAX &&
           command->stable_samples != 0U;
}

static bool abort_requested(const gimbal_autotune_command_t *command)
{
    return g_gimbal_autotune.command.abort_seq == command->request_seq;
}

static void stop_axes(void)
{
    vision_tracking_set_control_enabled(false);
    (void)stepper_service_stop_all();
}

static void release_control_without_stop(void)
{
    vision_tracking_set_control_enabled(false);
    (void)stepper_service_clear_pending_commands();
}

static void restore_normal_tracking(void)
{
    (void)stepper_service_clear_pending_commands();
    (void)stepper_service_set_axis_enabled(STEPPER_AXIS_YAW, true);
    (void)stepper_service_set_axis_enabled(STEPPER_AXIS_PITCH, true);
    vTaskDelay(pdMS_TO_TICKS(GIMBAL_AUTOTUNE_AXIS_ENABLE_DELAY_MS));
    vision_tracking_set_control_enabled(true);
}

static void run_boot_motion_diagnostic(void)
{
#if GIMBAL_AUTOTUNE_BOOT_DIAGNOSTIC_ENABLED
    vTaskDelay(pdMS_TO_TICKS(GIMBAL_AUTOTUNE_START_DELAY_MS));
    vision_tracking_set_control_enabled(false);
    vTaskDelay(pdMS_TO_TICKS(VISION_TRACKING_COMMAND_PERIOD_MS + 1U));
    (void)stepper_service_clear_pending_commands();
    (void)stepper_service_set_axis_enabled(STEPPER_AXIS_YAW, true);
    vTaskDelay(pdMS_TO_TICKS(GIMBAL_AUTOTUNE_AXIS_ENABLE_DELAY_MS));
    (void)move_axis(
        STEPPER_AXIS_YAW, GIMBAL_AUTOTUNE_BOOT_DIAGNOSTIC_PULSES);
    vTaskDelay(pdMS_TO_TICKS(1000U));
    (void)move_axis(
        STEPPER_AXIS_YAW, -GIMBAL_AUTOTUNE_BOOT_DIAGNOSTIC_PULSES);
    vTaskDelay(pdMS_TO_TICKS(1000U));
    restore_normal_tracking();
#endif
}

static bool take_control_for_offset(void)
{
    stepper_service_state_t yaw;
    stepper_service_state_t pitch;

    /* Let a vision iteration that already passed its enable check finish,
     * discard queued corrections, and stop the correction already executing
     * inside the motor driver before taking ownership. */
    vision_tracking_set_control_enabled(false);
    vTaskDelay(pdMS_TO_TICKS(VISION_TRACKING_COMMAND_PERIOD_MS + 1U));
    if (!stepper_service_stop_all()) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(GIMBAL_AUTOTUNE_STOP_SETTLE_MS));
    if (!stepper_service_get_axis_state(STEPPER_AXIS_YAW, &yaw) ||
        !stepper_service_get_axis_state(STEPPER_AXIS_PITCH, &pitch) ||
        yaw.command_pending || pitch.command_pending) {
        return false;
    }
    if (!stepper_service_set_axis_enabled(STEPPER_AXIS_YAW, true) ||
        !stepper_service_set_axis_enabled(STEPPER_AXIS_PITCH, true)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(GIMBAL_AUTOTUNE_AXIS_ENABLE_DELAY_MS));
    return stepper_service_get_axis_state(STEPPER_AXIS_YAW, &yaw) &&
           stepper_service_get_axis_state(STEPPER_AXIS_PITCH, &pitch) &&
           yaw.enabled && pitch.enabled && !yaw.command_pending &&
           !pitch.command_pending;
}

static void record_trace(uint32_t trial, uint32_t start_ms)
{
    uint32_t index = g_gimbal_autotune.trace_count;
    const volatile vision_tracking_debug_t *debug = &g_vision_tracking_debug;

    if (index >= GIMBAL_AUTOTUNE_MAX_TRACE_SAMPLES) {
        g_gimbal_autotune.trace_overflow = 1U;
        return;
    }
    g_gimbal_autotune.trace[index] = (gimbal_autotune_trace_t) {
        .trial = trial,
        .elapsed_ms = now_ms() - start_ms,
        .target_valid = debug->last_target_valid,
        .dx = debug->last_error_x_pixels,
        .dy = debug->last_error_y_pixels,
        .yaw_pulses = debug->yaw_output_pulses,
        .pitch_pulses = debug->pitch_output_pulses,
    };
    g_gimbal_autotune.trace_count = index + 1U;
}

static int32_t axes_move_status(uint32_t yaw_before, uint32_t pitch_before,
                                bool move_yaw, bool move_pitch)
{
    stepper_service_state_t yaw;
    stepper_service_state_t pitch;

    if (!stepper_service_get_axis_state(STEPPER_AXIS_YAW, &yaw) ||
        !stepper_service_get_axis_state(STEPPER_AXIS_PITCH, &pitch)) {
        return -1;
    }
    if ((!move_yaw || (yaw.transmitted_commands > yaw_before &&
                       yaw.last_tx_ok)) &&
        (!move_pitch || (pitch.transmitted_commands > pitch_before &&
                         pitch.last_tx_ok))) {
        return 1;
    }
    return 0;
}

static bool read_axis_position(stepper_axis_t axis, int32_t *position)
{
    stepper_service_state_t before;
    uint32_t start_ms;

    if ((position == NULL) ||
        !stepper_service_get_axis_state(axis, &before) ||
        !stepper_service_request_position(axis)) {
        return false;
    }
    start_ms = now_ms();
    while ((now_ms() - start_ms) <
           GIMBAL_AUTOTUNE_POSITION_QUERY_TIMEOUT_MS) {
        stepper_service_state_t current;

        if (stepper_service_get_axis_state(axis, &current) &&
            current.position_sequence != before.position_sequence) {
            *position = current.realtime_position;
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(GIMBAL_AUTOTUNE_POLL_PERIOD_MS));
    }
    return false;
}

static bool measured_offset_is_large_enough(int32_t before, int32_t after,
                                            int32_t requested_pulses)
{
    uint64_t measured_scaled =
        (uint64_t)abs_i32(after - before) *
        GIMBAL_AUTOTUNE_COMMAND_PULSES_PER_REV * 100U;
    uint64_t requested_scaled =
        (uint64_t)abs_i32(requested_pulses) *
        GIMBAL_AUTOTUNE_POSITION_COUNTS_PER_REV *
        GIMBAL_AUTOTUNE_MIN_POSITION_PERCENT;

    return measured_scaled >= requested_scaled;
}

static void undo_open_loop_offset(bool move_yaw, bool move_pitch,
                                  int32_t yaw_offset, int32_t pitch_offset)
{
    if (move_yaw) {
        (void)move_axis(STEPPER_AXIS_YAW, -yaw_offset);
    }
    if (move_pitch) {
        (void)move_axis(STEPPER_AXIS_PITCH, -pitch_offset);
    }
    vTaskDelay(pdMS_TO_TICKS(GIMBAL_AUTOTUNE_MOVE_SETTLE_MS));
}

static void update_axis_metrics(axis_metrics_t *metrics, int32_t error,
                                int32_t pulses, uint32_t elapsed_ms,
                                uint32_t stable_samples)
{
    uint32_t magnitude = abs_i32(error);
    int32_t sign = error_sign_outside_deadband(error);

    if (sign != 0) {
        if (metrics->previous_error_sign != 0 &&
            sign != metrics->previous_error_sign) {
            metrics->zero_crossings++;
        }
        metrics->previous_error_sign = sign;
    }
    if (metrics->have_previous_pulses) {
        uint32_t step = abs_i32(pulses - metrics->previous_pulses);
        if (step > metrics->max_pulse_step) {
            metrics->max_pulse_step = step;
        }
    }
    metrics->previous_pulses = pulses;
    metrics->have_previous_pulses = true;

    if (magnitude <= (uint32_t)VISION_TRACKING_DEADBAND_PIXELS) {
        metrics->stable_count++;
        metrics->stable_error_sum += magnitude;
        if (metrics->stable_count == stable_samples) {
            metrics->settle_ms = elapsed_ms;
        }
    } else {
        metrics->stable_count = 0U;
        metrics->stable_error_sum = 0U;
        metrics->settle_ms = 0U;
    }
}

static void store_axis_results(gimbal_autotune_trial_result_t *result,
                               const axis_metrics_t *yaw,
                               const axis_metrics_t *pitch)
{
    result->yaw_settle_ms = yaw->settle_ms;
    result->pitch_settle_ms = pitch->settle_ms;
    result->yaw_zero_crossings = yaw->zero_crossings;
    result->pitch_zero_crossings = pitch->zero_crossings;
    result->yaw_max_pulse_step = yaw->max_pulse_step;
    result->pitch_max_pulse_step = pitch->max_pulse_step;
    if (yaw->stable_count != 0U) {
        result->yaw_stable_mean_error =
            (float)yaw->stable_error_sum / (float)yaw->stable_count;
    }
    if (pitch->stable_count != 0U) {
        result->pitch_stable_mean_error =
            (float)pitch->stable_error_sum / (float)pitch->stable_count;
    }
}

static bool run_trial(uint32_t trial, uint32_t *random,
                      const gimbal_autotune_command_t *command)
{
    gimbal_autotune_trial_result_t *result =
        (gimbal_autotune_trial_result_t *)&g_gimbal_autotune.result[trial];
    stepper_service_state_t yaw_before;
    stepper_service_state_t pitch_before;
    axis_metrics_t yaw_metrics = {0};
    axis_metrics_t pitch_metrics = {0};
    bool move_yaw = command->yaw_max_pulses != 0U;
    bool move_pitch = command->pitch_max_pulses != 0U;
    uint32_t start_ms;

    *result = (gimbal_autotune_trial_result_t) {0};
    result->yaw_offset = move_yaw ?
        next_offset(random, command->yaw_max_pulses) : 0;
    result->pitch_offset = move_pitch ?
        next_offset(random, command->pitch_max_pulses) : 0;
    if (!take_control_for_offset()) {
        result->status = GIMBAL_AUTOTUNE_TRIAL_STATE_ERROR;
        release_control_without_stop();
        return false;
    }

    if (!stepper_service_get_axis_state(STEPPER_AXIS_YAW, &yaw_before) ||
        !stepper_service_get_axis_state(STEPPER_AXIS_PITCH, &pitch_before)) {
        result->status = GIMBAL_AUTOTUNE_TRIAL_STATE_ERROR;
        release_control_without_stop();
        return false;
    }
    if ((move_yaw && !read_axis_position(
                         STEPPER_AXIS_YAW, &result->yaw_position_before)) ||
        (move_pitch && !read_axis_position(
                           STEPPER_AXIS_PITCH,
                           &result->pitch_position_before))) {
        result->status = GIMBAL_AUTOTUNE_TRIAL_POSITION_ERROR;
        release_control_without_stop();
        return false;
    }
    if ((move_yaw && !move_axis(STEPPER_AXIS_YAW, result->yaw_offset)) ||
        (move_pitch && !move_axis(STEPPER_AXIS_PITCH,
                                  result->pitch_offset))) {
        result->status = GIMBAL_AUTOTUNE_TRIAL_MOVE_ERROR;
        release_control_without_stop();
        return false;
    }

    g_gimbal_autotune.phase = GIMBAL_AUTOTUNE_PHASE_WAIT_REACHED;
    start_ms = now_ms();
    while ((now_ms() - start_ms) < command->offset_timeout_ms) {
        int32_t move_status;

        if (abort_requested(command)) {
            result->status = GIMBAL_AUTOTUNE_TRIAL_ABORTED;
            stop_axes();
            return false;
        }
        move_status = axes_move_status(yaw_before.transmitted_commands,
                                       pitch_before.transmitted_commands,
                                       move_yaw, move_pitch);
        if (move_status < 0) {
            result->status = GIMBAL_AUTOTUNE_TRIAL_MOVE_ERROR;
            stop_axes();
            return false;
        }
        if (move_status > 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(GIMBAL_AUTOTUNE_POLL_PERIOD_MS));
    }
    result->offset_ms = now_ms() - start_ms;
    if (axes_move_status(yaw_before.transmitted_commands,
                         pitch_before.transmitted_commands,
                         move_yaw, move_pitch) <= 0) {
        result->status = GIMBAL_AUTOTUNE_TRIAL_OFFSET_TIMEOUT;
        stop_axes();
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(GIMBAL_AUTOTUNE_MOVE_SETTLE_MS));
    if (!stepper_service_get_axis_state(STEPPER_AXIS_YAW, &yaw_before) ||
        !stepper_service_get_axis_state(STEPPER_AXIS_PITCH, &pitch_before)) {
        result->status = GIMBAL_AUTOTUNE_TRIAL_STATE_ERROR;
        release_control_without_stop();
        return false;
    }
    result->yaw_response_function = yaw_before.last_response_function;
    result->yaw_response_code = yaw_before.last_response_code;
    result->pitch_response_function = pitch_before.last_response_function;
    result->pitch_response_code = pitch_before.last_response_code;
    if ((move_yaw && !read_axis_position(
                         STEPPER_AXIS_YAW, &result->yaw_position_after)) ||
        (move_pitch && !read_axis_position(
                           STEPPER_AXIS_PITCH,
                           &result->pitch_position_after))) {
        result->status = GIMBAL_AUTOTUNE_TRIAL_POSITION_ERROR;
        release_control_without_stop();
        return false;
    }
    if ((move_yaw && !measured_offset_is_large_enough(
                         result->yaw_position_before,
                         result->yaw_position_after,
                         result->yaw_offset)) ||
        (move_pitch && !measured_offset_is_large_enough(
                           result->pitch_position_before,
                           result->pitch_position_after,
                           result->pitch_offset))) {
        result->status = GIMBAL_AUTOTUNE_TRIAL_MOVE_ERROR;
        release_control_without_stop();
        return false;
    }
    result->yaw_max_error =
        (float)abs_i32(g_vision_tracking_debug.last_error_x_pixels);
    result->pitch_max_error =
        (float)abs_i32(g_vision_tracking_debug.last_error_y_pixels);
    if (!g_vision_tracking_debug.last_target_valid ||
        (move_yaw && result->yaw_max_error <=
            (float)VISION_TRACKING_DEADBAND_PIXELS) ||
        (move_pitch && result->pitch_max_error <=
             (float)VISION_TRACKING_DEADBAND_PIXELS)) {
        result->status = 5U;
        undo_open_loop_offset(move_yaw, move_pitch,
                              result->yaw_offset, result->pitch_offset);
        release_control_without_stop();
        return false;
    }

    g_gimbal_autotune.phase = GIMBAL_AUTOTUNE_PHASE_TRACK;
    vision_tracking_set_control_enabled(true);
    start_ms = now_ms();
    while ((now_ms() - start_ms) < command->track_timeout_ms) {
        const volatile vision_tracking_debug_t *debug =
            &g_vision_tracking_debug;
        uint32_t elapsed_ms = now_ms() - start_ms;
        uint32_t yaw_error = abs_i32(debug->last_error_x_pixels);
        uint32_t pitch_error = abs_i32(debug->last_error_y_pixels);

        if (abort_requested(command)) {
            result->status = GIMBAL_AUTOTUNE_TRIAL_ABORTED;
            stop_axes();
            return false;
        }
        record_trace(trial, start_ms);
        if (!debug->last_target_valid) {
            result->target_lost_samples++;
            yaw_metrics.stable_count = 0U;
            yaw_metrics.stable_error_sum = 0U;
            pitch_metrics.stable_count = 0U;
            pitch_metrics.stable_error_sum = 0U;
        } else {
            result->valid_samples++;
            if ((float)yaw_error > result->yaw_max_error) {
                result->yaw_max_error = (float)yaw_error;
            }
            if ((float)pitch_error > result->pitch_max_error) {
                result->pitch_max_error = (float)pitch_error;
            }
            update_axis_metrics(&yaw_metrics,
                                debug->last_error_x_pixels,
                                debug->yaw_output_pulses,
                                elapsed_ms, command->stable_samples);
            update_axis_metrics(&pitch_metrics,
                                debug->last_error_y_pixels,
                                debug->pitch_output_pulses,
                                elapsed_ms, command->stable_samples);
            if ((!move_yaw || yaw_metrics.settle_ms != 0U) &&
                (!move_pitch || pitch_metrics.settle_ms != 0U)) {
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(GIMBAL_AUTOTUNE_POLL_PERIOD_MS));
    }

    result->return_ms = now_ms() - start_ms;
    store_axis_results(result, &yaw_metrics, &pitch_metrics);
    if ((move_yaw && yaw_metrics.settle_ms == 0U) ||
        (move_pitch && pitch_metrics.settle_ms == 0U) ||
        result->target_lost_samples * GIMBAL_AUTOTUNE_POLL_PERIOD_MS >
            command->target_lost_ms) {
        result->status = GIMBAL_AUTOTUNE_TRIAL_TRACK_TIMEOUT;
        stop_axes();
        return false;
    }
    result->status = GIMBAL_AUTOTUNE_TRIAL_OK;
    g_gimbal_autotune.phase = GIMBAL_AUTOTUNE_PHASE_SETTLED;
    return true;
}

void gimbal_autotune_task(void *argument)
{
    uint32_t accepted_request = 0U;
    uint32_t last_observation_ms = 0U;

    (void)argument;
    run_boot_motion_diagnostic();
    for (;;) {
        gimbal_autotune_command_t command;
        uint32_t request_before = g_gimbal_autotune.command.request_seq;
        uint32_t request_after;
        uint32_t trial;
        uint32_t random;
        bool aborted = false;

        if (request_before == 0U || request_before == accepted_request ||
            g_gimbal_autotune.status == GIMBAL_AUTOTUNE_RUNNING) {
            if ((now_ms() - last_observation_ms) >=
                GIMBAL_AUTOTUNE_OBSERVATION_PERIOD_MS) {
                publish_observation();
                last_observation_ms = now_ms();
            }
            vTaskDelay(pdMS_TO_TICKS(GIMBAL_AUTOTUNE_POLL_PERIOD_MS));
            continue;
        }
        command = g_gimbal_autotune.command;
        request_after = g_gimbal_autotune.command.request_seq;
        if (request_before != request_after || !command_valid(&command)) {
            snapshot_begin();
            g_gimbal_autotune.status = GIMBAL_AUTOTUNE_FAILED;
            g_gimbal_autotune.ack_seq = request_before;
            snapshot_end();
            accepted_request = request_before;
            continue;
        }

        accepted_request = request_before;
        snapshot_begin();
        g_gimbal_autotune.ack_seq = request_before;
        g_gimbal_autotune.status = GIMBAL_AUTOTUNE_RUNNING;
        g_gimbal_autotune.phase = GIMBAL_AUTOTUNE_PHASE_IDLE;
        g_gimbal_autotune.current_trial = 0U;
        g_gimbal_autotune.failed_trials = 0U;
        g_gimbal_autotune.trace_count = 0U;
        g_gimbal_autotune.trace_overflow = 0U;
        g_gimbal_autotune.applied_yaw_kp = command.yaw_kp;
        g_gimbal_autotune.applied_yaw_kd = command.yaw_kd;
        g_gimbal_autotune.applied_pitch_kp = command.pitch_kp;
        g_gimbal_autotune.applied_pitch_kd = command.pitch_kd;
        for (trial = 0U; trial < GIMBAL_AUTOTUNE_MAX_TRIALS; trial++) {
            g_gimbal_autotune.result[trial] =
                (gimbal_autotune_trial_result_t) { .status = UINT32_MAX };
        }
        snapshot_end();

        (void)stepper_service_set_axis_enabled(STEPPER_AXIS_YAW, true);
        (void)stepper_service_set_axis_enabled(STEPPER_AXIS_PITCH, true);
        (void)vision_tracking_set_tuning(&(vision_tracking_tuning_t) {
            .yaw_kp = command.yaw_kp,
            .yaw_kd = command.yaw_kd,
            .pitch_kp = command.pitch_kp,
            .pitch_kd = command.pitch_kd,
        });
        random = command.seed;
        for (trial = 0U; trial < command.trial_count; trial++) {
            g_gimbal_autotune.current_trial = trial;
            if (!run_trial(trial, &random, &command)) {
                g_gimbal_autotune.failed_trials++;
                break;
            }
            if (abort_requested(&command)) {
                aborted = true;
                break;
            }
        }
        restore_normal_tracking();
        snapshot_begin();
        g_gimbal_autotune.status = aborted ? GIMBAL_AUTOTUNE_FAILED :
                                             GIMBAL_AUTOTUNE_COMPLETE;
        g_gimbal_autotune.phase = GIMBAL_AUTOTUNE_PHASE_IDLE;
        snapshot_end();
    }
}
