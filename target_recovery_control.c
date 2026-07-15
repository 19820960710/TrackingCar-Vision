#include "target_recovery_control.h"

#include <stddef.h>

static int32_t scan_axis_command(int32_t position, int32_t half_span,
                                 int32_t step, int8_t *direction)
{
    int32_t command;
    int32_t remaining;

    if (position >= half_span) {
        *direction = -1;
    } else if (position <= -half_span) {
        *direction = 1;
    }

    command = (*direction > 0) ? step : -step;
    remaining = (*direction > 0) ? (half_span - position) :
        (-half_span - position);
    if (((*direction > 0) && (command > remaining)) ||
        ((*direction < 0) && (command < remaining))) {
        command = remaining;
    }
    return command;
}

static void begin_scanning(target_recovery_control_t *control,
                           uint32_t now_ms)
{
    control->mode = TARGET_RECOVERY_SCANNING;
    control->yaw_scan_position_pulses = 0;
    control->pitch_scan_position_pulses = 0;
    control->yaw_scan_direction = 1;
    control->pitch_scan_direction = 1;
    control->yaw_last_scan_command_ms = now_ms;
    control->pitch_last_scan_command_ms = now_ms;
}

void target_recovery_control_init(
    target_recovery_control_t *control,
    const target_recovery_config_t *config)
{
    if (control == NULL) {
        return;
    }

    control->config = config;
    control->mode = TARGET_RECOVERY_TRACKING;
    control->target_lost_at_ms = 0U;
    control->yaw_last_scan_command_ms = 0U;
    control->pitch_last_scan_command_ms = 0U;
    control->last_valid_dx = 0;
    control->last_valid_dy = 0;
    control->yaw_scan_position_pulses = 0;
    control->pitch_scan_position_pulses = 0;
    control->yaw_scan_direction = 1;
    control->pitch_scan_direction = 1;
    control->has_valid_target_history = false;
}

void target_recovery_control_update(
    target_recovery_control_t *control, bool target_valid,
    int16_t dx, int16_t dy, uint32_t now_ms,
    target_recovery_output_t *output)
{
    target_recovery_mode_t previous_mode;

    if ((control == NULL) || (control->config == NULL) ||
        (output == NULL)) {
        return;
    }

    output->mode = control->mode;
    output->use_tracker = false;
    output->reset_trackers = false;
    output->stop_trackers = false;
    output->tracking_dx = 0;
    output->tracking_dy = 0;
    output->yaw_scan_pulses = 0;
    output->pitch_scan_pulses = 0;

    if (target_valid) {
        previous_mode = control->mode;
        control->mode = TARGET_RECOVERY_TRACKING;
        control->last_valid_dx = dx;
        control->last_valid_dy = dy;
        control->has_valid_target_history = true;
        output->mode = control->mode;
        output->use_tracker = true;
        output->reset_trackers =
            previous_mode != TARGET_RECOVERY_TRACKING;
        output->stop_trackers =
            previous_mode == TARGET_RECOVERY_SCANNING;
        output->tracking_dx = dx;
        output->tracking_dy = dy;
        return;
    }

    if (control->mode == TARGET_RECOVERY_TRACKING) {
        control->mode = TARGET_RECOVERY_PREDICTING;
        control->target_lost_at_ms = now_ms;
    }

    if (control->mode == TARGET_RECOVERY_PREDICTING) {
        if (control->has_valid_target_history &&
            ((uint32_t) (now_ms - control->target_lost_at_ms) <
             control->config->prediction_hold_ms)) {
            output->mode = control->mode;
            output->use_tracker = true;
            output->tracking_dx = control->last_valid_dx;
            output->tracking_dy = control->last_valid_dy;
            return;
        }

        begin_scanning(control, now_ms);
        output->mode = control->mode;
        output->stop_trackers = true;
        return;
    }

    output->mode = control->mode;
    if ((uint32_t) (now_ms - control->yaw_last_scan_command_ms) >=
        control->config->scan_command_period_ms) {
        output->yaw_scan_pulses = scan_axis_command(
            control->yaw_scan_position_pulses,
            control->config->yaw_scan_half_span_pulses,
            control->config->yaw_scan_step_pulses,
            &control->yaw_scan_direction);
    }
    if ((uint32_t) (now_ms - control->pitch_last_scan_command_ms) >=
        control->config->scan_command_period_ms) {
        output->pitch_scan_pulses = scan_axis_command(
            control->pitch_scan_position_pulses,
            control->config->pitch_scan_half_span_pulses,
            control->config->pitch_scan_step_pulses,
            &control->pitch_scan_direction);
    }
}

void target_recovery_commit_scan(target_recovery_control_t *control,
                                 int32_t yaw_pulses, bool yaw_accepted,
                                 int32_t pitch_pulses,
                                 bool pitch_accepted,
                                 uint32_t now_ms)
{
    if ((control == NULL) ||
        (control->mode != TARGET_RECOVERY_SCANNING)) {
        return;
    }

    if (yaw_accepted) {
        control->yaw_scan_position_pulses += yaw_pulses;
        control->yaw_last_scan_command_ms = now_ms;
    }
    if (pitch_accepted) {
        control->pitch_scan_position_pulses += pitch_pulses;
        control->pitch_last_scan_command_ms = now_ms;
    }
}
