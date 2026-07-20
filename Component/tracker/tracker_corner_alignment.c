#include "tracker_corner_alignment.h"

#include <stddef.h>

#define TRACKER_CORNER_SCAN_FIRST_CHANNEL 2u
#define TRACKER_CORNER_SCAN_LAST_CHANNEL  5u
#define TRACKER_CORNER_PI_F               3.14159265358979323846f

static float tracker_corner_alignment_absf(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float tracker_corner_alignment_clampf(float value,
                                             float minimum,
                                             float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static uint8_t tracker_corner_alignment_start_turn(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input);

uint8_t tracker_corner_alignment_scan_center_line_detected(
    const uint16_t normalized[TRACKER_CORNER_CHANNEL_COUNT],
    uint16_t normalization_max,
    uint16_t line_threshold)
{
    uint8_t channel;

    if ((normalized == NULL) ||
        (normalization_max == 0u) ||
        (line_threshold == 0u)) {
        return 0u;
    }

    for (channel = TRACKER_CORNER_SCAN_FIRST_CHANNEL;
         channel <= TRACKER_CORNER_SCAN_LAST_CHANNEL;
         channel++) {
        uint16_t brightness = normalized[channel];
        uint16_t black_strength;

        if (brightness > normalization_max) {
            brightness = normalization_max;
        }
        black_strength = (uint16_t)(normalization_max - brightness);
        if (black_strength >= line_threshold) {
            return 1u;
        }
    }

    return 0u;
}

static void tracker_corner_alignment_set_targets(
    tracker_corner_alignment_t *controller,
    float left_target_speed_mm_s,
    float right_target_speed_mm_s)
{
    controller->target_speed_mm_s[TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL] =
        left_target_speed_mm_s;
    controller->target_speed_mm_s[TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL] =
        right_target_speed_mm_s;
}

static void tracker_corner_alignment_set_targets_slewed(
    tracker_corner_alignment_t *controller,
    float left_target_speed_mm_s,
    float right_target_speed_mm_s,
    float slew_limit_per_step)
{
    controller->target_speed_mm_s[
        TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL] =
        tracker_corner_alignment_clampf(
            left_target_speed_mm_s,
            controller->target_speed_mm_s[
                TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL] -
                slew_limit_per_step,
            controller->target_speed_mm_s[
                TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL] +
                slew_limit_per_step);
    controller->target_speed_mm_s[
        TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL] =
        tracker_corner_alignment_clampf(
            right_target_speed_mm_s,
            controller->target_speed_mm_s[
                TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL] -
                slew_limit_per_step,
            controller->target_speed_mm_s[
                TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL] +
                slew_limit_per_step);
}

static void tracker_corner_alignment_publish(
    const tracker_corner_alignment_t *controller,
    tracker_corner_alignment_output_t *output)
{
    output->left_target_speed_mm_s =
        controller->target_speed_mm_s[
            TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL];
    output->right_target_speed_mm_s =
        controller->target_speed_mm_s[
            TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL];
    output->mode = controller->mode;
    output->abort_reason = controller->abort_reason;
    output->diagnostic_result = controller->diagnostic_result;
    output->diagnostic_latched = controller->diagnostic_latched;
    output->overriding =
        tracker_corner_alignment_is_overriding(controller);
}

static uint8_t tracker_corner_alignment_event_is_valid(
    tracker_corner_feature_t feature,
    tracker_corner_direction_t direction)
{
    uint8_t feature_is_supported =
        (uint8_t)((feature == TRACKER_CORNER_FEATURE_RIGHT_ANGLE) ||
                  (feature == TRACKER_CORNER_FEATURE_ACUTE_ANGLE));

    return (uint8_t)((feature_is_supported != 0u) &&
                     (direction != TRACKER_CORNER_DIRECTION_UNKNOWN));
}

static uint8_t tracker_corner_alignment_feature_execution_enabled(
    const tracker_corner_alignment_t *controller,
    tracker_corner_feature_t feature)
{
    if ((controller == NULL) ||
        (feature == TRACKER_CORNER_FEATURE_NONE)) {
        return 0u;
    }
    if ((feature == TRACKER_CORNER_FEATURE_ACUTE_ANGLE) &&
        (controller->config.acute_execution_enable == 0u)) {
        return 0u;
    }
    return 1u;
}

static uint8_t tracker_corner_alignment_config_is_valid(
    const tracker_corner_alignment_config_t *config)
{
    gear_motor_angle_controller_t test_controller;

    if ((config == NULL) ||
        (config->candidate_target_speed_mm_s <= 0.0f) ||
        (config->forward_target_speed_mm_s <= 0.0f) ||
        (config->max_probe_distance_mm <= 0.0f) ||
        (config->backoff_distance_mm <= 0.0f) ||
        (config->backoff_target_speed_mm_s <= 0.0f) ||
        (config->line_loss_acute_fallback_enable > 1u) ||
        (config->acute_execution_enable > 1u) ||
        ((config->line_loss_acute_fallback_enable != 0u) &&
         (config->forced_turn_direction !=
          TRACKER_CORNER_DIRECTION_NEGATIVE) &&
         (config->forced_turn_direction !=
          TRACKER_CORNER_DIRECTION_POSITIVE)) ||
        (config->candidate_release_frames == 0u) ||
        (config->candidate_min_hold_cycles == 0u) ||
        (config->candidate_timeout_cycles == 0u) ||
        (config->candidate_min_hold_cycles >
         config->candidate_timeout_cycles) ||
        (config->probe_timeout_cycles == 0u) ||
        (config->backoff_timeout_cycles == 0u) ||
        (config->right_angle_inner_distance_mm <= 0.0f) ||
        (config->right_angle_outer_distance_mm <= 0.0f) ||
        (tracker_corner_alignment_absf(
             config->acute_angle_inner_distance_mm) <= 0.0f) ||
        (config->acute_angle_outer_distance_mm <= 0.0f) ||
        (config->turn_ready_cycles == 0u) ||
        (config->turn_timeout_cycles == 0u) ||
        (config->turn_complete_cycles == 0u) ||
        (config->post_turn_scan_enable > 1u) ||
        ((config->post_turn_scan_enable != 0u) &&
         ((config->post_turn_scan_track_width_mm <= 0.0f) ||
          (config->post_turn_scan_offset_deg <= 0.0f) ||
          (config->post_turn_scan_line_threshold == 0u))) ||
        (config->release_centered_error_limit <= 0.0f) ||
        (config->release_stable_cycles == 0u) ||
        (config->right_angle_direct.enable > 1u) ||
        ((config->right_angle_direct.enable != 0u) &&
         ((config->right_angle_direct
               .candidate_target_speed_mm_s <= 0.0f) ||
          (config->right_angle_direct.outer_max_speed_mm_s <= 0.0f) ||
          (config->right_angle_direct.target_slew_limit_per_step <= 0.0f) ||
          (config->right_angle_direct.minimum_alignment_progress <= 0.0f) ||
          (config->right_angle_direct.minimum_alignment_progress > 1.0f) ||
          (config->right_angle_direct.alignment_base_speed_mm_s <= 0.0f) ||
          (config->right_angle_direct.alignment_kp <= 0.0f) ||
          (config->right_angle_direct.alignment_correction_limit <= 0.0f) ||
          (config->right_angle_direct
               .alignment_correction_slew_limit_per_step <= 0.0f) ||
          (config->right_angle_direct.release_error_delta_limit <= 0.0f) ||
          (config->right_angle_direct.release_stable_cycles == 0u) ||
          (config->right_angle_direct
               .alignment_line_loss_timeout_cycles == 0u)))) {
        return 0u;
    }

    return gear_motor_angle_control_init(
        &test_controller, &config->wheel_angle_config);
}

static void tracker_corner_alignment_clear_runtime(
    tracker_corner_alignment_t *controller,
    uint32_t event_count)
{
    uint8_t wheel;

    controller->mode = TRACKER_CORNER_ALIGNMENT_WAIT_EVENT;
    controller->abort_reason = TRACKER_CORNER_ALIGNMENT_ABORT_NONE;
    controller->feature = TRACKER_CORNER_FEATURE_NONE;
    controller->direction = TRACKER_CORNER_DIRECTION_UNKNOWN;
    controller->scan_phase = TRACKER_CORNER_SCAN_IDLE;
    controller->last_consumed_event_count = event_count;
    controller->elapsed_cycles = 0u;
    controller->backoff_settle_cycles = 0u;
    controller->release_centered_cycles = 0u;
    controller->right_angle_line_loss_cycles = 0u;
    controller->candidate_reject_frames = 0u;
    controller->backoff_line_reacquired = 0u;
    controller->right_angle_has_last_error = 0u;
    controller->right_angle_alignment_correction = 0.0f;
    controller->right_angle_last_error = 0.0f;
    tracker_corner_alignment_set_targets(controller, 0.0f, 0.0f);

    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        controller->probe_start_angle_rad[wheel] = 0.0f;
        controller->line_loss_angle_rad[wheel] = 0.0f;
        controller->probe_distance_mm[wheel] = 0.0f;
        controller->backoff_progress_mm[wheel] = 0.0f;
        controller->turn_distance_mm[wheel] = 0.0f;
        controller->backoff_reached[wheel] = 0u;
        gear_motor_angle_control_reset(
            &controller->wheel_controller[wheel]);
    }
}

static tracker_corner_diagnostic_t
tracker_corner_alignment_diagnostic_from_abort(
    tracker_corner_alignment_mode_t previous_mode,
    tracker_corner_alignment_abort_t reason)
{
    if (previous_mode == TRACKER_CORNER_ALIGNMENT_CANDIDATE_HOLD) {
        return (reason ==
                TRACKER_CORNER_ALIGNMENT_ABORT_CANDIDATE_LINE_LOSS)
                   ? TRACKER_CORNER_DIAGNOSTIC_CANDIDATE_LINE_LOSS
                   : TRACKER_CORNER_DIAGNOSTIC_CANDIDATE_UNCONFIRMED;
    }

    switch (reason) {
    case TRACKER_CORNER_ALIGNMENT_ABORT_PROBE_DISTANCE:
    case TRACKER_CORNER_ALIGNMENT_ABORT_PROBE_TIMEOUT:
        return TRACKER_CORNER_DIAGNOSTIC_PROBE_FAULT;
    case TRACKER_CORNER_ALIGNMENT_ABORT_BACKOFF_TIMEOUT:
        return TRACKER_CORNER_DIAGNOSTIC_BACKOFF_FAULT;
    case TRACKER_CORNER_ALIGNMENT_ABORT_TURN_START:
    case TRACKER_CORNER_ALIGNMENT_ABORT_TURN_TIMEOUT:
        return TRACKER_CORNER_DIAGNOSTIC_TURN_FAULT;
    case TRACKER_CORNER_ALIGNMENT_ABORT_CANDIDATE_TIMEOUT:
        return TRACKER_CORNER_DIAGNOSTIC_CANDIDATE_UNCONFIRMED;
    case TRACKER_CORNER_ALIGNMENT_ABORT_CANDIDATE_LINE_LOSS:
        return TRACKER_CORNER_DIAGNOSTIC_CANDIDATE_LINE_LOSS;
    case TRACKER_CORNER_ALIGNMENT_ABORT_NONE:
    default:
        return TRACKER_CORNER_DIAGNOSTIC_NONE;
    }
}

static void tracker_corner_alignment_enter_abort(
    tracker_corner_alignment_t *controller,
    tracker_corner_alignment_abort_t reason)
{
    controller->diagnostic_result =
        tracker_corner_alignment_diagnostic_from_abort(
            controller->mode, reason);
    controller->diagnostic_latched = 1u;
    controller->mode = TRACKER_CORNER_ALIGNMENT_ABORT;
    controller->abort_reason = reason;
    controller->abort_count++;
    tracker_corner_alignment_set_targets(controller, 0.0f, 0.0f);
}

static void tracker_corner_alignment_start_probe(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    uint8_t wheel;

    controller->mode = TRACKER_CORNER_ALIGNMENT_FORWARD_PROBE;
    controller->abort_reason = TRACKER_CORNER_ALIGNMENT_ABORT_NONE;
    controller->feature = input->detected_feature;
    controller->direction = input->detected_direction;
    controller->diagnostic_result =
        TRACKER_CORNER_DIAGNOSTIC_NONE;
    controller->diagnostic_latched = 0u;
    controller->elapsed_cycles = 0u;
    controller->activation_count++;
    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        controller->probe_start_angle_rad[wheel] =
            input->wheel_angle_rad[wheel];
        controller->line_loss_angle_rad[wheel] =
            input->wheel_angle_rad[wheel];
        controller->probe_distance_mm[wheel] = 0.0f;
    }
    tracker_corner_alignment_set_targets(
        controller,
        controller->config.forward_target_speed_mm_s,
        controller->config.forward_target_speed_mm_s);
}

static void tracker_corner_alignment_start_candidate(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    uint8_t wheel;

    controller->mode = TRACKER_CORNER_ALIGNMENT_CANDIDATE_HOLD;
    controller->abort_reason = TRACKER_CORNER_ALIGNMENT_ABORT_NONE;
    controller->feature = input->candidate_feature;
    controller->direction = input->candidate_direction;
    controller->diagnostic_result =
        TRACKER_CORNER_DIAGNOSTIC_CANDIDATE_UNCONFIRMED;
    controller->diagnostic_latched = 0u;
    controller->elapsed_cycles = 0u;
    controller->candidate_reject_frames = 0u;
    controller->candidate_attempt_count++;
    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        controller->probe_start_angle_rad[wheel] =
            input->wheel_angle_rad[wheel];
        controller->line_loss_angle_rad[wheel] =
            input->wheel_angle_rad[wheel];
        controller->probe_distance_mm[wheel] = 0.0f;
    }
    if ((controller->feature == TRACKER_CORNER_FEATURE_RIGHT_ANGLE) &&
        (controller->config.right_angle_direct.enable != 0u)) {
        tracker_corner_alignment_set_targets(
            controller,
            controller->config.right_angle_direct
                .candidate_target_speed_mm_s,
            controller->config.right_angle_direct
                .candidate_target_speed_mm_s);
    } else {
        tracker_corner_alignment_set_targets(
            controller,
            controller->config.candidate_target_speed_mm_s,
            controller->config.candidate_target_speed_mm_s);
    }
}

static void tracker_corner_alignment_confirm_candidate(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    controller->mode = TRACKER_CORNER_ALIGNMENT_FORWARD_PROBE;
    controller->feature = input->detected_feature;
    controller->direction = input->detected_direction;
    controller->diagnostic_result =
        TRACKER_CORNER_DIAGNOSTIC_NONE;
    controller->diagnostic_latched = 0u;
    controller->elapsed_cycles = 0u;
    controller->candidate_reject_frames = 0u;
    controller->activation_count++;
    tracker_corner_alignment_set_targets(
        controller,
        controller->config.forward_target_speed_mm_s,
        controller->config.forward_target_speed_mm_s);
}

static uint8_t tracker_corner_alignment_probe_distance_exceeded(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    float wheel_radius_mm =
        controller->config.wheel_angle_config.wheel_radius_mm;
    uint8_t wheel;

    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        controller->probe_distance_mm[wheel] =
            tracker_corner_alignment_absf(
                input->wheel_angle_rad[wheel] -
                controller->probe_start_angle_rad[wheel]) *
            wheel_radius_mm;
        if (controller->probe_distance_mm[wheel] >
            controller->config.max_probe_distance_mm) {
            return 1u;
        }
    }

    return 0u;
}

static void tracker_corner_alignment_start_backoff(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    uint8_t wheel;

    if (controller->config.line_loss_acute_fallback_enable != 0u) {
        controller->feature = TRACKER_CORNER_FEATURE_ACUTE_ANGLE;
        controller->direction =
            controller->config.forced_turn_direction;
    }

    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        controller->line_loss_angle_rad[wheel] =
            input->wheel_angle_rad[wheel];
        controller->backoff_progress_mm[wheel] = 0.0f;
        controller->backoff_reached[wheel] = 0u;
        gear_motor_angle_control_reset(
            &controller->wheel_controller[wheel]);
    }

    controller->mode = TRACKER_CORNER_ALIGNMENT_BACKOFF;
    controller->elapsed_cycles = 0u;
    controller->backoff_settle_cycles = 0u;
    controller->backoff_line_reacquired = 0u;
    tracker_corner_alignment_set_targets(controller, 0.0f, 0.0f);
}

static void tracker_corner_alignment_update_probe(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    if (controller->elapsed_cycles < UINT16_MAX) {
        controller->elapsed_cycles++;
    }
    if (tracker_corner_alignment_probe_distance_exceeded(
            controller, input) != 0u) {
        tracker_corner_alignment_enter_abort(
            controller,
            TRACKER_CORNER_ALIGNMENT_ABORT_PROBE_DISTANCE);
        return;
    }

    if (controller->elapsed_cycles >=
        controller->config.probe_timeout_cycles) {
        tracker_corner_alignment_enter_abort(
            controller,
            TRACKER_CORNER_ALIGNMENT_ABORT_PROBE_TIMEOUT);
        return;
    }

    if (input->line_detected == 0u) {
        tracker_corner_alignment_start_backoff(
            controller, input);
    }
}

static void tracker_corner_alignment_update_candidate(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    uint8_t candidate_matches;
    uint8_t confirmed_event = 0u;

    if (controller->elapsed_cycles < UINT16_MAX) {
        controller->elapsed_cycles++;
    }
    if (tracker_corner_alignment_probe_distance_exceeded(
            controller, input) != 0u) {
        tracker_corner_alignment_enter_abort(
            controller,
            TRACKER_CORNER_ALIGNMENT_ABORT_PROBE_DISTANCE);
        return;
    }
    if (input->detector_event_count !=
        controller->last_consumed_event_count) {
        if (tracker_corner_alignment_event_is_valid(
                input->detected_feature,
                input->detected_direction) != 0u) {
            confirmed_event = 1u;
        } else {
            controller->last_consumed_event_count =
                input->detector_event_count;
        }
    }

    if ((confirmed_event != 0u) &&
        (input->detected_feature ==
         TRACKER_CORNER_FEATURE_RIGHT_ANGLE) &&
        (controller->config.right_angle_direct.enable != 0u)) {
        controller->last_consumed_event_count =
            input->detector_event_count;
        controller->feature = input->detected_feature;
        controller->direction = input->detected_direction;
        controller->diagnostic_result =
            TRACKER_CORNER_DIAGNOSTIC_NONE;
        controller->diagnostic_latched = 0u;
        controller->activation_count++;
        if (tracker_corner_alignment_start_turn(
                controller, input) == 0u) {
            tracker_corner_alignment_enter_abort(
                controller,
                TRACKER_CORNER_ALIGNMENT_ABORT_TURN_START);
        }
        return;
    }

    if (input->line_detected == 0u) {
        if ((controller->config
                 .line_loss_acute_fallback_enable != 0u) ||
            (confirmed_event != 0u) ||
            (controller->feature ==
             TRACKER_CORNER_FEATURE_ACUTE_ANGLE)) {
            if (confirmed_event != 0u) {
                controller->last_consumed_event_count =
                    input->detector_event_count;
                controller->feature = input->detected_feature;
                controller->direction = input->detected_direction;
            }
            controller->diagnostic_result =
                TRACKER_CORNER_DIAGNOSTIC_NONE;
            controller->diagnostic_latched = 0u;
            controller->activation_count++;
            controller->mode =
                TRACKER_CORNER_ALIGNMENT_FORWARD_PROBE;
            tracker_corner_alignment_start_backoff(
                controller, input);
        } else {
            tracker_corner_alignment_enter_abort(
                controller,
                TRACKER_CORNER_ALIGNMENT_ABORT_CANDIDATE_LINE_LOSS);
        }
        return;
    }

    if (confirmed_event != 0u) {
        if (controller->elapsed_cycles >=
            controller->config.candidate_min_hold_cycles) {
            controller->last_consumed_event_count =
                input->detector_event_count;
            tracker_corner_alignment_confirm_candidate(
                controller, input);
        }
        return;
    }

    if (controller->elapsed_cycles >=
        controller->config.candidate_timeout_cycles) {
        tracker_corner_alignment_enter_abort(
            controller,
            TRACKER_CORNER_ALIGNMENT_ABORT_CANDIDATE_TIMEOUT);
        return;
    }

    candidate_matches = (uint8_t)(
        (input->candidate_frames > 0u) &&
        (input->candidate_feature == controller->feature) &&
        (input->candidate_direction == controller->direction));
    if (candidate_matches != 0u) {
        controller->candidate_reject_frames = 0u;
        return;
    }

    if (controller->candidate_reject_frames < UINT8_MAX) {
        controller->candidate_reject_frames++;
    }
    if (controller->candidate_reject_frames >=
        controller->config.candidate_release_frames) {
        controller->candidate_release_count++;
        tracker_corner_alignment_clear_runtime(
            controller, input->detector_event_count);
    }
}

static void tracker_corner_alignment_update_backoff(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    float wheel_radius_mm =
        controller->config.wheel_angle_config.wheel_radius_mm;
    float position_tolerance_mm =
        controller->config.wheel_angle_config.angle_tolerance_rad *
        wheel_radius_mm;
    float stop_speed_tolerance_mm_s =
        controller->config.wheel_angle_config
            .stop_speed_tolerance_rad_s *
        wheel_radius_mm;
    uint8_t all_stopped = 1u;
    uint8_t wheel;

    if (controller->elapsed_cycles < UINT16_MAX) {
        controller->elapsed_cycles++;
    }
    if (input->line_detected != 0u) {
        controller->backoff_line_reacquired = 1u;
    }
    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        controller->backoff_progress_mm[wheel] =
            (controller->line_loss_angle_rad[wheel] -
             input->wheel_angle_rad[wheel]) *
            wheel_radius_mm;
        if ((controller->backoff_reached[wheel] == 0u) &&
            (controller->backoff_progress_mm[wheel] >=
             controller->config.backoff_distance_mm -
                 position_tolerance_mm)) {
            controller->backoff_reached[wheel] = 1u;
        }

        if ((controller->backoff_line_reacquired != 0u) ||
            (controller->backoff_reached[wheel] != 0u)) {
            controller->target_speed_mm_s[wheel] = 0.0f;
        } else {
            controller->target_speed_mm_s[wheel] =
                -controller->config.backoff_target_speed_mm_s;
        }
        if (tracker_corner_alignment_absf(
                input->measured_speed_mm_s[wheel]) >
            stop_speed_tolerance_mm_s) {
            all_stopped = 0u;
        }
    }

    if ((controller->backoff_line_reacquired != 0u) &&
        (all_stopped != 0u)) {
        if (controller->backoff_settle_cycles < UINT16_MAX) {
            controller->backoff_settle_cycles++;
        }
        if (controller->backoff_settle_cycles >=
            controller->config.wheel_angle_config.settle_cycles) {
            controller->mode = TRACKER_CORNER_ALIGNMENT_TURN_READY;
            controller->elapsed_cycles = 0u;
            tracker_corner_alignment_set_targets(
                controller, 0.0f, 0.0f);
            return;
        }
    } else {
        controller->backoff_settle_cycles = 0u;
    }

    if (controller->elapsed_cycles >=
        controller->config.backoff_timeout_cycles) {
        tracker_corner_alignment_enter_abort(
            controller,
            TRACKER_CORNER_ALIGNMENT_ABORT_BACKOFF_TIMEOUT);
    }
}

static uint8_t tracker_corner_alignment_select_turn_distances(
    tracker_corner_alignment_t *controller)
{
    float inner_distance_mm;
    float outer_distance_mm;
    float scan_offset_distance_mm;

    if (controller->feature == TRACKER_CORNER_FEATURE_RIGHT_ANGLE) {
        inner_distance_mm =
            controller->config.right_angle_inner_distance_mm;
        outer_distance_mm =
            controller->config.right_angle_outer_distance_mm;
    } else if (controller->feature ==
               TRACKER_CORNER_FEATURE_ACUTE_ANGLE) {
        inner_distance_mm =
            controller->config.acute_angle_inner_distance_mm;
        outer_distance_mm =
            controller->config.acute_angle_outer_distance_mm;
    } else {
        return 0u;
    }

    /* Hardware trials established NEGATIVE as a physical left turn. */
    if (controller->direction == TRACKER_CORNER_DIRECTION_NEGATIVE) {
        controller->turn_distance_mm[
            TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL] = inner_distance_mm;
        controller->turn_distance_mm[
            TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL] = outer_distance_mm;
    } else if (controller->direction ==
               TRACKER_CORNER_DIRECTION_POSITIVE) {
        controller->turn_distance_mm[
            TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL] = outer_distance_mm;
        controller->turn_distance_mm[
            TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL] = inner_distance_mm;
    } else {
        return 0u;
    }

    if ((controller->config.post_turn_scan_enable != 0u) &&
        (controller->feature == TRACKER_CORNER_FEATURE_ACUTE_ANGLE)) {
        scan_offset_distance_mm =
            0.5f * controller->config.post_turn_scan_track_width_mm *
            controller->config.post_turn_scan_offset_deg *
            TRACKER_CORNER_PI_F / 180.0f;
        if (controller->direction ==
            TRACKER_CORNER_DIRECTION_POSITIVE) {
            controller->turn_distance_mm[
                TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL] +=
                scan_offset_distance_mm;
            controller->turn_distance_mm[
                TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL] -=
                scan_offset_distance_mm;
        } else {
            controller->turn_distance_mm[
                TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL] -=
                scan_offset_distance_mm;
            controller->turn_distance_mm[
                TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL] +=
                scan_offset_distance_mm;
        }
    }

    return 1u;
}

static uint8_t tracker_corner_alignment_start_turn(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    float max_distance_mm;
    float wheel_radius_mm =
        controller->config.wheel_angle_config.wheel_radius_mm;
    float base_max_angular_speed_rad_s =
        controller->config.wheel_angle_config
            .max_angular_speed_rad_s;
    uint8_t wheel;

    if (tracker_corner_alignment_select_turn_distances(
            controller) == 0u) {
        return 0u;
    }

    max_distance_mm = tracker_corner_alignment_absf(
        controller->turn_distance_mm[
            TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL]);
    if (tracker_corner_alignment_absf(
            controller->turn_distance_mm[
                TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL]) >
        max_distance_mm) {
        max_distance_mm = tracker_corner_alignment_absf(
            controller->turn_distance_mm[
                TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL]);
    }
    if (max_distance_mm <= 0.0f) {
        return 0u;
    }

    if ((controller->feature ==
         TRACKER_CORNER_FEATURE_RIGHT_ANGLE) &&
        (controller->config.right_angle_direct.enable != 0u)) {
        base_max_angular_speed_rad_s =
            controller->config.right_angle_direct
                .outer_max_speed_mm_s /
            wheel_radius_mm;
    }

    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        float distance_abs_mm = tracker_corner_alignment_absf(
            controller->turn_distance_mm[wheel]);

        if (distance_abs_mm <= 0.0f) {
            return 0u;
        }
        controller->wheel_controller[wheel].config
            .max_angular_speed_rad_s =
            base_max_angular_speed_rad_s *
            distance_abs_mm / max_distance_mm;
        if (gear_motor_angle_control_start_relative(
                &controller->wheel_controller[wheel],
                input->wheel_angle_rad[wheel],
                controller->turn_distance_mm[wheel] /
                    wheel_radius_mm) == 0u) {
            uint8_t reset_wheel;

            for (reset_wheel = 0u;
                 reset_wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
                 reset_wheel++) {
                gear_motor_angle_control_reset(
                    &controller->wheel_controller[reset_wheel]);
            }
            return 0u;
        }
    }

    controller->mode = TRACKER_CORNER_ALIGNMENT_TURNING;
    controller->scan_phase =
        ((controller->config.post_turn_scan_enable != 0u) &&
         (controller->feature == TRACKER_CORNER_FEATURE_ACUTE_ANGLE))
            ? TRACKER_CORNER_SCAN_TO_50_DEG
            : TRACKER_CORNER_SCAN_IDLE;
    controller->elapsed_cycles = 0u;
    controller->release_centered_cycles = 0u;
    controller->right_angle_line_loss_cycles = 0u;
    controller->right_angle_has_last_error = 0u;
    controller->right_angle_alignment_correction = 0.0f;
    controller->right_angle_last_error = 0.0f;
    controller->turn_count++;
    if ((controller->feature ==
         TRACKER_CORNER_FEATURE_RIGHT_ANGLE) &&
        (controller->config.right_angle_direct.enable != 0u)) {
        if ((controller->target_speed_mm_s[
                 TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL] == 0.0f) &&
            (controller->target_speed_mm_s[
                 TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL] == 0.0f)) {
            tracker_corner_alignment_set_targets(
                controller,
                controller->config.right_angle_direct
                    .candidate_target_speed_mm_s,
                controller->config.right_angle_direct
                    .candidate_target_speed_mm_s);
        }
    } else {
        tracker_corner_alignment_set_targets(controller, 0.0f, 0.0f);
    }
    return 1u;
}

static void tracker_corner_alignment_enter_tracking_release(
    tracker_corner_alignment_t *controller)
{
    uint8_t wheel;

    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        gear_motor_angle_control_reset(
            &controller->wheel_controller[wheel]);
    }
    controller->diagnostic_result =
        TRACKER_CORNER_DIAGNOSTIC_NONE;
    controller->diagnostic_latched = 0u;
    controller->mode =
        TRACKER_CORNER_ALIGNMENT_TRACKING_RELEASE;
    controller->scan_phase = TRACKER_CORNER_SCAN_IDLE;
    controller->elapsed_cycles = 0u;
    controller->release_centered_cycles = 0u;
    tracker_corner_alignment_set_targets(controller, 0.0f, 0.0f);
}

static uint8_t tracker_corner_alignment_start_scan_segment(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input,
    tracker_corner_alignment_scan_phase_t phase)
{
    float body_delta_deg;
    float wheel_distance_mm;
    float direction_sign;
    float wheel_radius_mm =
        controller->config.wheel_angle_config.wheel_radius_mm;
    uint8_t wheel;

    direction_sign =
        (controller->direction == TRACKER_CORNER_DIRECTION_POSITIVE)
            ? 1.0f
            : -1.0f;
    switch (phase) {
    case TRACKER_CORNER_SCAN_TO_40_DEG:
        body_delta_deg = -2.0f * direction_sign *
                         controller->config.post_turn_scan_offset_deg;
        break;
    case TRACKER_CORNER_SCAN_RETURN_45_DEG:
        body_delta_deg = direction_sign *
                         controller->config.post_turn_scan_offset_deg;
        break;
    case TRACKER_CORNER_SCAN_TO_50_DEG:
    case TRACKER_CORNER_SCAN_IDLE:
    default:
        return 0u;
    }

    wheel_distance_mm =
        0.5f * controller->config.post_turn_scan_track_width_mm *
        body_delta_deg * TRACKER_CORNER_PI_F / 180.0f;
    controller->turn_distance_mm[
        TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL] = wheel_distance_mm;
    controller->turn_distance_mm[
        TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL] = -wheel_distance_mm;

    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        controller->wheel_controller[wheel].config
            .max_angular_speed_rad_s =
            controller->config.wheel_angle_config
                .max_angular_speed_rad_s;
        if (gear_motor_angle_control_start_relative(
                &controller->wheel_controller[wheel],
                input->wheel_angle_rad[wheel],
                controller->turn_distance_mm[wheel] /
                    wheel_radius_mm) == 0u) {
            uint8_t reset_wheel;

            for (reset_wheel = 0u;
                 reset_wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
                 reset_wheel++) {
                gear_motor_angle_control_reset(
                    &controller->wheel_controller[reset_wheel]);
            }
            return 0u;
        }
    }

    controller->scan_phase = phase;
    controller->mode =
        TRACKER_CORNER_ALIGNMENT_POST_TURN_SCAN;
    tracker_corner_alignment_set_targets(controller, 0.0f, 0.0f);
    return 1u;
}

static uint8_t tracker_corner_alignment_start_post_turn_scan(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    controller->elapsed_cycles = 0u;
    if (tracker_corner_alignment_start_scan_segment(
            controller,
            input,
            TRACKER_CORNER_SCAN_TO_40_DEG) == 0u) {
        return 0u;
    }
    controller->scan_count++;
    return 1u;
}

static void tracker_corner_alignment_update_turn_ready(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    tracker_corner_alignment_set_targets(controller, 0.0f, 0.0f);
    if (controller->elapsed_cycles < UINT16_MAX) {
        controller->elapsed_cycles++;
    }
    if (controller->elapsed_cycles >=
        controller->config.turn_ready_cycles) {
        if (tracker_corner_alignment_start_turn(
                controller, input) == 0u) {
            tracker_corner_alignment_enter_abort(
                controller,
                TRACKER_CORNER_ALIGNMENT_ABORT_TURN_START);
        }
    }
}

static float tracker_corner_alignment_right_angle_progress(
    const tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    float wheel_radius_mm =
        controller->config.wheel_angle_config.wheel_radius_mm;
    float minimum_progress = 1.0f;
    uint8_t wheel;

    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        float distance_mm = tracker_corner_alignment_absf(
            (input->wheel_angle_rad[wheel] -
             controller->wheel_controller[wheel].start_angle_rad) *
            wheel_radius_mm);
        float target_distance_mm = tracker_corner_alignment_absf(
            controller->turn_distance_mm[wheel]);
        float progress;

        if (target_distance_mm <= 0.0f) {
            return 0.0f;
        }
        progress = distance_mm / target_distance_mm;
        if (progress < minimum_progress) {
            minimum_progress = progress;
        }
    }

    return minimum_progress;
}

static void tracker_corner_alignment_enter_right_angle_alignment(
    tracker_corner_alignment_t *controller)
{
    uint8_t wheel;

    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        gear_motor_angle_control_reset(
            &controller->wheel_controller[wheel]);
    }
    controller->mode =
        TRACKER_CORNER_ALIGNMENT_RIGHT_ANGLE_ALIGNING;
    controller->release_centered_cycles = 0u;
    controller->right_angle_line_loss_cycles = 0u;
    controller->right_angle_has_last_error = 0u;
    controller->right_angle_alignment_correction = 0.0f;
    controller->right_angle_last_error = 0.0f;
}

static void tracker_corner_alignment_update_right_angle_alignment(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    const tracker_corner_right_angle_direct_config_t *config =
        &controller->config.right_angle_direct;
    float desired_correction;
    uint8_t stable_error;

    if (input->line_detected == 0u) {
        controller->release_centered_cycles = 0u;
        controller->right_angle_has_last_error = 0u;
        if (controller->right_angle_line_loss_cycles < UINT16_MAX) {
            controller->right_angle_line_loss_cycles++;
        }
        if (controller->right_angle_line_loss_cycles >=
            config->alignment_line_loss_timeout_cycles) {
            tracker_corner_alignment_enter_abort(
                controller,
                TRACKER_CORNER_ALIGNMENT_ABORT_TURN_TIMEOUT);
            return;
        }
    } else {
        controller->right_angle_line_loss_cycles = 0u;
        desired_correction = tracker_corner_alignment_clampf(
            config->alignment_kp * input->line_error,
            -config->alignment_correction_limit,
            config->alignment_correction_limit);
        controller->right_angle_alignment_correction =
            tracker_corner_alignment_clampf(
                desired_correction,
                controller->right_angle_alignment_correction -
                    config->alignment_correction_slew_limit_per_step,
                controller->right_angle_alignment_correction +
                    config->alignment_correction_slew_limit_per_step);
        stable_error = (uint8_t)(
            (tracker_corner_alignment_absf(input->line_error) <=
             controller->config.release_centered_error_limit) &&
            (controller->right_angle_has_last_error != 0u) &&
            (tracker_corner_alignment_absf(
                 input->line_error -
                 controller->right_angle_last_error) <=
             config->release_error_delta_limit));
        if (stable_error != 0u) {
            if (controller->release_centered_cycles < UINT16_MAX) {
                controller->release_centered_cycles++;
            }
        } else {
            controller->release_centered_cycles = 0u;
        }
        controller->right_angle_last_error = input->line_error;
        controller->right_angle_has_last_error = 1u;
        if (controller->release_centered_cycles >=
            config->release_stable_cycles) {
            tracker_corner_alignment_enter_tracking_release(
                controller);
            return;
        }
    }

    tracker_corner_alignment_set_targets_slewed(
        controller,
        config->alignment_base_speed_mm_s +
            controller->right_angle_alignment_correction,
        config->alignment_base_speed_mm_s -
            controller->right_angle_alignment_correction,
        config->target_slew_limit_per_step);
}

static void tracker_corner_alignment_update_turn(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    gear_motor_angle_output_t output[TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT];
    float wheel_radius_mm =
        controller->config.wheel_angle_config.wheel_radius_mm;
    uint8_t wheel;

    if (controller->elapsed_cycles < UINT16_MAX) {
        controller->elapsed_cycles++;
    }
    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        gear_motor_angle_control_step(
            &controller->wheel_controller[wheel],
            input->wheel_angle_rad[wheel],
            input->measured_speed_mm_s[wheel] / wheel_radius_mm,
            &output[wheel]);
        if ((controller->feature ==
             TRACKER_CORNER_FEATURE_RIGHT_ANGLE) &&
            (controller->config.right_angle_direct.enable != 0u)) {
            continue;
        }
        controller->target_speed_mm_s[wheel] =
            output[wheel].target_linear_speed_mm_s;
    }

    if ((controller->feature ==
         TRACKER_CORNER_FEATURE_RIGHT_ANGLE) &&
        (controller->config.right_angle_direct.enable != 0u)) {
        tracker_corner_alignment_set_targets_slewed(
            controller,
            output[TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL]
                .target_linear_speed_mm_s,
            output[TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL]
                .target_linear_speed_mm_s,
            controller->config.right_angle_direct
                .target_slew_limit_per_step);
        if ((tracker_corner_alignment_right_angle_progress(
                 controller, input) >=
             controller->config.right_angle_direct
                 .minimum_alignment_progress) &&
            (input->line_detected != 0u) &&
            (input->scan_center_line_detected != 0u) &&
            (input->candidate_frames == 0u)) {
            tracker_corner_alignment_enter_right_angle_alignment(
                controller);
            tracker_corner_alignment_update_right_angle_alignment(
                controller, input);
            return;
        }
    }

    if ((gear_motor_angle_control_is_complete(
             &controller->wheel_controller[
                 TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL]) != 0u) &&
        (gear_motor_angle_control_is_complete(
             &controller->wheel_controller[
                 TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL]) != 0u)) {
        controller->turn_complete_count++;
        tracker_corner_alignment_set_targets(controller, 0.0f, 0.0f);
        if ((controller->config.post_turn_scan_enable != 0u) &&
            (controller->feature ==
             TRACKER_CORNER_FEATURE_ACUTE_ANGLE)) {
            if (input->scan_center_line_detected != 0u) {
                controller->scan_line_found_count++;
                tracker_corner_alignment_enter_tracking_release(
                    controller);
            } else if (tracker_corner_alignment_start_post_turn_scan(
                           controller, input) == 0u) {
                tracker_corner_alignment_enter_abort(
                    controller,
                    TRACKER_CORNER_ALIGNMENT_ABORT_TURN_START);
            }
            return;
        }
        controller->mode = TRACKER_CORNER_ALIGNMENT_TURN_COMPLETE;
        controller->elapsed_cycles = 0u;
        return;
    }

    if (controller->elapsed_cycles >=
        controller->config.turn_timeout_cycles) {
        tracker_corner_alignment_enter_abort(
            controller,
            TRACKER_CORNER_ALIGNMENT_ABORT_TURN_TIMEOUT);
    }
}

static void tracker_corner_alignment_update_post_turn_scan(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    gear_motor_angle_output_t
        output[TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT];
    float wheel_radius_mm =
        controller->config.wheel_angle_config.wheel_radius_mm;
    uint8_t wheel;

    if (input->scan_center_line_detected != 0u) {
        controller->scan_line_found_count++;
        tracker_corner_alignment_enter_tracking_release(controller);
        return;
    }
    if (controller->elapsed_cycles < UINT16_MAX) {
        controller->elapsed_cycles++;
    }

    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        gear_motor_angle_control_step(
            &controller->wheel_controller[wheel],
            input->wheel_angle_rad[wheel],
            input->measured_speed_mm_s[wheel] / wheel_radius_mm,
            &output[wheel]);
        controller->target_speed_mm_s[wheel] =
            output[wheel].target_linear_speed_mm_s;
    }

    if ((gear_motor_angle_control_is_complete(
             &controller->wheel_controller[
                 TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL]) != 0u) &&
        (gear_motor_angle_control_is_complete(
             &controller->wheel_controller[
                 TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL]) != 0u)) {
        tracker_corner_alignment_scan_phase_t next_phase;

        if (controller->scan_phase ==
            TRACKER_CORNER_SCAN_TO_40_DEG) {
            next_phase = TRACKER_CORNER_SCAN_RETURN_45_DEG;
        } else {
            controller->mode =
                TRACKER_CORNER_ALIGNMENT_TURN_COMPLETE;
            controller->scan_phase = TRACKER_CORNER_SCAN_IDLE;
            controller->elapsed_cycles = 0u;
            tracker_corner_alignment_set_targets(
                controller, 0.0f, 0.0f);
            return;
        }

        if (tracker_corner_alignment_start_scan_segment(
                controller, input, next_phase) == 0u) {
            tracker_corner_alignment_enter_abort(
                controller,
                TRACKER_CORNER_ALIGNMENT_ABORT_TURN_START);
            return;
        }
    }

    if (controller->elapsed_cycles >=
        controller->config.turn_timeout_cycles) {
        tracker_corner_alignment_enter_abort(
            controller,
            TRACKER_CORNER_ALIGNMENT_ABORT_TURN_TIMEOUT);
    }
}

static void tracker_corner_alignment_update_turn_complete(
    tracker_corner_alignment_t *controller)
{
    tracker_corner_alignment_set_targets(controller, 0.0f, 0.0f);
    if (controller->elapsed_cycles < UINT16_MAX) {
        controller->elapsed_cycles++;
    }
    if (controller->elapsed_cycles >=
        controller->config.turn_complete_cycles) {
        tracker_corner_alignment_enter_tracking_release(controller);
    }
}

static void tracker_corner_alignment_update_tracking_release(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input)
{
    tracker_corner_alignment_set_targets(controller, 0.0f, 0.0f);
    if ((input->line_detected != 0u) &&
        (input->candidate_frames == 0u) &&
        (tracker_corner_alignment_absf(input->line_error) <=
         controller->config.release_centered_error_limit)) {
        if (controller->release_centered_cycles < UINT16_MAX) {
            controller->release_centered_cycles++;
        }
    } else {
        controller->release_centered_cycles = 0u;
    }

    if (controller->release_centered_cycles >=
        controller->config.release_stable_cycles) {
        tracker_corner_alignment_clear_runtime(
            controller, input->detector_event_count);
    }
}

uint8_t tracker_corner_alignment_init(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_config_t *config,
    uint32_t initial_event_count)
{
    uint8_t wheel;

    if (controller == NULL) {
        return 0u;
    }

    *controller = (tracker_corner_alignment_t){0};
    if (tracker_corner_alignment_config_is_valid(config) == 0u) {
        return 0u;
    }

    controller->config = *config;
    for (wheel = 0u;
         wheel < TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT;
         wheel++) {
        if (gear_motor_angle_control_init(
                &controller->wheel_controller[wheel],
                &config->wheel_angle_config) == 0u) {
            *controller = (tracker_corner_alignment_t){0};
            return 0u;
        }
    }
    controller->initialized = 1u;
    tracker_corner_alignment_clear_runtime(
        controller, initial_event_count);
    return 1u;
}

void tracker_corner_alignment_reset(
    tracker_corner_alignment_t *controller,
    uint32_t event_count)
{
    if ((controller == NULL) || (controller->initialized == 0u)) {
        return;
    }

    controller->diagnostic_result =
        TRACKER_CORNER_DIAGNOSTIC_NONE;
    controller->diagnostic_latched = 0u;
    tracker_corner_alignment_clear_runtime(controller, event_count);
}

void tracker_corner_alignment_update(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input,
    tracker_corner_alignment_output_t *output)
{
    if (output == NULL) {
        return;
    }
    *output = (tracker_corner_alignment_output_t){0};
    if ((controller == NULL) || (input == NULL) ||
        (controller->initialized == 0u)) {
        return;
    }

    controller->update_count++;
    if ((controller->mode != TRACKER_CORNER_ALIGNMENT_WAIT_EVENT) &&
        (controller->mode != TRACKER_CORNER_ALIGNMENT_CANDIDATE_HOLD) &&
        (input->detector_event_count !=
         controller->last_consumed_event_count)) {
        controller->last_consumed_event_count =
            input->detector_event_count;
    }

    switch (controller->mode) {
    case TRACKER_CORNER_ALIGNMENT_WAIT_EVENT:
        tracker_corner_alignment_set_targets(controller, 0.0f, 0.0f);
        if ((controller->config
                 .line_loss_acute_fallback_enable != 0u) &&
            (input->line_sample_valid != 0u) &&
            (input->line_detected == 0u)) {
            controller->abort_reason =
                TRACKER_CORNER_ALIGNMENT_ABORT_NONE;
            controller->diagnostic_result =
                TRACKER_CORNER_DIAGNOSTIC_NONE;
            controller->diagnostic_latched = 0u;
            controller->activation_count++;
            tracker_corner_alignment_start_backoff(
                controller, input);
        } else if (input->detector_event_count !=
            controller->last_consumed_event_count) {
            controller->last_consumed_event_count =
                input->detector_event_count;
            if ((tracker_corner_alignment_event_is_valid(
                     input->detected_feature,
                     input->detected_direction) != 0u) &&
                (tracker_corner_alignment_feature_execution_enabled(
                     controller, input->detected_feature) != 0u)) {
                if ((input->detected_feature ==
                     TRACKER_CORNER_FEATURE_RIGHT_ANGLE) &&
                    (controller->config.right_angle_direct.enable != 0u)) {
                    controller->feature = input->detected_feature;
                    controller->direction = input->detected_direction;
                    controller->activation_count++;
                    if (tracker_corner_alignment_start_turn(
                            controller, input) == 0u) {
                        tracker_corner_alignment_enter_abort(
                            controller,
                            TRACKER_CORNER_ALIGNMENT_ABORT_TURN_START);
                    }
                } else {
                    tracker_corner_alignment_start_probe(
                        controller, input);
                }
            }
        } else if ((input->candidate_frames > 0u) &&
                   (tracker_corner_alignment_event_is_valid(
                        input->candidate_feature,
                        input->candidate_direction) != 0u) &&
                   (tracker_corner_alignment_feature_execution_enabled(
                        controller, input->candidate_feature) != 0u)) {
            tracker_corner_alignment_start_candidate(
                controller, input);
        }
        break;

    case TRACKER_CORNER_ALIGNMENT_CANDIDATE_HOLD:
        tracker_corner_alignment_update_candidate(controller, input);
        break;

    case TRACKER_CORNER_ALIGNMENT_FORWARD_PROBE:
        tracker_corner_alignment_update_probe(controller, input);
        break;

    case TRACKER_CORNER_ALIGNMENT_BACKOFF:
        tracker_corner_alignment_update_backoff(controller, input);
        break;

    case TRACKER_CORNER_ALIGNMENT_TURN_READY:
        tracker_corner_alignment_update_turn_ready(controller, input);
        break;

    case TRACKER_CORNER_ALIGNMENT_TURNING:
        tracker_corner_alignment_update_turn(controller, input);
        break;

    case TRACKER_CORNER_ALIGNMENT_RIGHT_ANGLE_ALIGNING:
        tracker_corner_alignment_update_right_angle_alignment(
            controller, input);
        break;

    case TRACKER_CORNER_ALIGNMENT_POST_TURN_SCAN:
        tracker_corner_alignment_update_post_turn_scan(
            controller, input);
        break;

    case TRACKER_CORNER_ALIGNMENT_TURN_COMPLETE:
        tracker_corner_alignment_update_turn_complete(controller);
        break;

    case TRACKER_CORNER_ALIGNMENT_TRACKING_RELEASE:
        tracker_corner_alignment_update_tracking_release(
            controller, input);
        break;

    case TRACKER_CORNER_ALIGNMENT_ABORT:
    default:
        tracker_corner_alignment_set_targets(controller, 0.0f, 0.0f);
        break;
    }

    tracker_corner_alignment_publish(controller, output);
}

uint8_t tracker_corner_alignment_is_overriding(
    const tracker_corner_alignment_t *controller)
{
    if ((controller == NULL) || (controller->initialized == 0u)) {
        return 0u;
    }

    return (uint8_t)(
        (controller->mode != TRACKER_CORNER_ALIGNMENT_WAIT_EVENT) &&
        (controller->mode !=
         TRACKER_CORNER_ALIGNMENT_TRACKING_RELEASE));
}
