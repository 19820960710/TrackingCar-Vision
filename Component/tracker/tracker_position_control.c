#include "tracker/tracker_position_control.h"

#include <stddef.h>

static float clampf(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static float absf_local(float value)
{
    return (value < 0.0f) ? -value : value;
}

void tracker_position_control_reset(tracker_position_controller_t *controller)
{
    tracker_position_config_t saved_config;

    if (controller == NULL) {
        return;
    }
    saved_config = controller->config;
    *controller = (tracker_position_controller_t){0};
    controller->config = saved_config;
}

void tracker_position_control_init(tracker_position_controller_t *controller,
                                   const tracker_position_config_t *config)
{
    if ((controller == NULL) || (config == NULL)) {
        return;
    }
    controller->config = *config;
    tracker_position_control_reset(controller);
}

void tracker_position_control_update(tracker_position_controller_t *controller,
                                     float error,
                                     uint8_t line_detected,
                                     float dt_s)
{
    float candidate_integral;
    float derivative_alpha;
    float desired_base_speed;
    uint8_t was_tracking;

    if (controller == NULL) {
        return;
    }
    if ((line_detected == 0u) || (dt_s <= 0.0f)) {
        tracker_position_control_reset(controller);
        return;
    }

    was_tracking = controller->has_last_error;
    controller->line_detected = 1u;
    controller->error = error;
    candidate_integral = clampf(controller->integral + error * dt_s,
                                -controller->config.integral_limit,
                                controller->config.integral_limit);
    controller->raw_derivative = (was_tracking != 0u)
        ? (error - controller->last_error) / dt_s
        : 0.0f;
    if (controller->config.derivative_filter_time_constant_s > 0.0f) {
        derivative_alpha = dt_s /
            (controller->config.derivative_filter_time_constant_s + dt_s);
        controller->derivative += derivative_alpha *
            (controller->raw_derivative - controller->derivative);
    } else {
        controller->derivative = controller->raw_derivative;
    }

    controller->derivative_term = controller->config.kd * controller->derivative;
    controller->control_error = clampf(
        error + controller->config.error_prediction_time_s *
                    controller->derivative,
        -1.0f, 1.0f);
    controller->edge_term = controller->config.edge_gain *
                            controller->control_error *
                            controller->control_error *
                            controller->control_error;
    controller->proportional_term =
        controller->config.kp * controller->control_error;
    controller->raw_correction = controller->proportional_term +
                                 controller->config.ki * candidate_integral +
                                 controller->derivative_term +
                                 controller->edge_term;
    if (((controller->raw_correction < controller->config.correction_limit) ||
         (error < 0.0f)) &&
        ((controller->raw_correction > -controller->config.correction_limit) ||
         (error > 0.0f))) {
        controller->integral = candidate_integral;
    }

    controller->integral_term = controller->config.ki * controller->integral;
    controller->raw_correction = controller->proportional_term +
                                 controller->integral_term +
                                 controller->derivative_term +
                                 controller->edge_term;
    controller->amplitude_limited_correction = clampf(
        controller->raw_correction,
        -controller->config.correction_limit,
        controller->config.correction_limit);
    if (controller->config.correction_slew_limit_per_step > 0.0f) {
        controller->correction = clampf(
            controller->amplitude_limited_correction,
            controller->correction -
                controller->config.correction_slew_limit_per_step,
            controller->correction +
                controller->config.correction_slew_limit_per_step);
    } else {
        controller->correction = controller->amplitude_limited_correction;
    }

    desired_base_speed = clampf(
        controller->config.base_speed -
            controller->config.error_slowdown_gain *
                absf_local(controller->control_error) -
            controller->config.derivative_slowdown_gain *
                absf_local(controller->derivative),
        controller->config.minimum_effective_base_speed,
        controller->config.base_speed);
    if ((controller->config.effective_base_slew_limit_per_step > 0.0f) &&
        (was_tracking != 0u)) {
        controller->effective_base_speed = clampf(
            desired_base_speed,
            controller->effective_base_speed -
                controller->config.effective_base_slew_limit_per_step,
            controller->effective_base_speed +
                controller->config.effective_base_slew_limit_per_step);
    } else {
        controller->effective_base_speed = desired_base_speed;
    }
    controller->left_target_speed = clampf(
        controller->effective_base_speed + controller->correction,
        controller->config.min_target_speed,
        controller->config.max_target_speed);
    controller->right_target_speed = clampf(
        controller->effective_base_speed - controller->correction,
        controller->config.min_target_speed,
        controller->config.max_target_speed);
    controller->last_error = error;
    controller->has_last_error = 1u;
    controller->update_count++;
}
