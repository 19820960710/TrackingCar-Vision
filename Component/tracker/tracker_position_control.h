#ifndef TRACKER_POSITION_CONTROL_H
#define TRACKER_POSITION_CONTROL_H

#include <stdint.h>

typedef struct {
    float kp;
    float ki;
    float kd;
    float edge_gain;
    float integral_limit;
    float correction_limit;
    float base_speed;
    float min_target_speed;
    float max_target_speed;
    float derivative_filter_time_constant_s;
    float error_prediction_time_s;
    float correction_slew_limit_per_step;
    float error_slowdown_gain;
    float derivative_slowdown_gain;
    float minimum_effective_base_speed;
    float effective_base_slew_limit_per_step;
} tracker_position_config_t;

typedef struct {
    tracker_position_config_t config;
    float error;
    float last_error;
    float integral;
    float raw_derivative;
    float derivative;
    float control_error;
    float proportional_term;
    float integral_term;
    float derivative_term;
    float edge_term;
    float raw_correction;
    float amplitude_limited_correction;
    float correction;
    float effective_base_speed;
    float left_target_speed;
    float right_target_speed;
    uint32_t update_count;
    uint8_t line_detected;
    uint8_t has_last_error;
} tracker_position_controller_t;

void tracker_position_control_init(tracker_position_controller_t *controller,
                                   const tracker_position_config_t *config);
void tracker_position_control_reset(tracker_position_controller_t *controller);
void tracker_position_control_update(tracker_position_controller_t *controller,
                                     float error,
                                     uint8_t line_detected,
                                     float dt_s);

#endif
