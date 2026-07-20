#include "gear_motor_angle_control.h"

#include <stddef.h>

static float gear_motor_angle_absf(float value)
{
    return (value < 0.0f) ? -value : value;
}
static float gear_motor_angle_clampf(float value,
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

static uint8_t gear_motor_angle_config_is_valid(
    const gear_motor_angle_config_t *config)
{
    if (config == NULL) {
        return 0u;
    }

    return (uint8_t)((config->kp > 0.0f) &&
                     (config->wheel_radius_mm > 0.0f) &&
                     (config->max_angular_speed_rad_s > 0.0f) &&
                     (config->angle_tolerance_rad > 0.0f) &&
                     (config->stop_speed_tolerance_rad_s >= 0.0f) &&
                     (config->settle_cycles > 0u));
}

static void gear_motor_angle_clear_runtime(
    gear_motor_angle_controller_t *controller)
{
    controller->start_angle_rad = 0.0f;
    controller->target_angle_rad = 0.0f;
    controller->current_angle_rad = 0.0f;
    controller->measured_angular_speed_rad_s = 0.0f;
    controller->error_rad = 0.0f;
    controller->target_angular_speed_rad_s = 0.0f;
    controller->target_linear_speed_mm_s = 0.0f;
    controller->settled_cycles = 0u;
    controller->status = GEAR_MOTOR_ANGLE_STATUS_IDLE;
}

static void gear_motor_angle_publish_output(
    const gear_motor_angle_controller_t *controller,
    gear_motor_angle_output_t *output)
{
    output->target_angular_speed_rad_s =
        controller->target_angular_speed_rad_s;
    output->target_linear_speed_mm_s =
        controller->target_linear_speed_mm_s;
    output->status = controller->status;
}

uint8_t gear_motor_angle_control_init(
    gear_motor_angle_controller_t *controller,
    const gear_motor_angle_config_t *config)
{
    if (controller == NULL) {
        return 0u;
    }

    *controller = (gear_motor_angle_controller_t){0};
    if (gear_motor_angle_config_is_valid(config) == 0u) {
        return 0u;
    }

    controller->config = *config;
    controller->initialized = 1u;
    gear_motor_angle_clear_runtime(controller);
    return 1u;
}

void gear_motor_angle_control_reset(
    gear_motor_angle_controller_t *controller)
{
    if (controller == NULL) {
        return;
    }

    gear_motor_angle_clear_runtime(controller);
}

uint8_t gear_motor_angle_control_start_relative(
    gear_motor_angle_controller_t *controller,
    float current_angle_rad,
    float delta_angle_rad)
{
    if ((controller == NULL) || (controller->initialized == 0u)) {
        return 0u;
    }

    gear_motor_angle_clear_runtime(controller);
    controller->start_angle_rad = current_angle_rad;
    controller->target_angle_rad = current_angle_rad + delta_angle_rad;
    controller->current_angle_rad = current_angle_rad;
    controller->error_rad = delta_angle_rad;
    controller->status = GEAR_MOTOR_ANGLE_STATUS_RUNNING;
    return 1u;
}

void gear_motor_angle_control_step(
    gear_motor_angle_controller_t *controller,
    float current_angle_rad,
    float measured_angular_speed_rad_s,
    gear_motor_angle_output_t *output)
{
    float raw_target_angular_speed;

    if (output == NULL) {
        return;
    }

    *output = (gear_motor_angle_output_t){0};
    if ((controller == NULL) || (controller->initialized == 0u)) {
        return;
    }

    if (controller->status != GEAR_MOTOR_ANGLE_STATUS_RUNNING) {
        gear_motor_angle_publish_output(controller, output);
        return;
    }

    controller->current_angle_rad = current_angle_rad;
    controller->measured_angular_speed_rad_s =
        measured_angular_speed_rad_s;
    controller->error_rad = controller->target_angle_rad -
                            controller->current_angle_rad;

    if (gear_motor_angle_absf(controller->error_rad) <=
        controller->config.angle_tolerance_rad) {
        controller->target_angular_speed_rad_s = 0.0f;
        controller->target_linear_speed_mm_s = 0.0f;

        if (gear_motor_angle_absf(measured_angular_speed_rad_s) <=
            controller->config.stop_speed_tolerance_rad_s) {
            if (controller->settled_cycles <
                controller->config.settle_cycles) {
                controller->settled_cycles++;
            }
            if (controller->settled_cycles >=
                controller->config.settle_cycles) {
                controller->status =
                    GEAR_MOTOR_ANGLE_STATUS_COMPLETE;
            }
        } else {
            controller->settled_cycles = 0u;
        }

        gear_motor_angle_publish_output(controller, output);
        return;
    }

    controller->settled_cycles = 0u;
    raw_target_angular_speed = controller->config.kp *
                               controller->error_rad;
    controller->target_angular_speed_rad_s = gear_motor_angle_clampf(
        raw_target_angular_speed,
        -controller->config.max_angular_speed_rad_s,
        controller->config.max_angular_speed_rad_s);
    controller->target_linear_speed_mm_s =
        controller->target_angular_speed_rad_s *
        controller->config.wheel_radius_mm;
    gear_motor_angle_publish_output(controller, output);
}

uint8_t gear_motor_angle_control_is_complete(
    const gear_motor_angle_controller_t *controller)
{
    if (controller == NULL) {
        return 0u;
    }

    return (uint8_t)(controller->status ==
                     GEAR_MOTOR_ANGLE_STATUS_COMPLETE);
}

