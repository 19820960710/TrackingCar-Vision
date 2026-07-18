/**
 * @file gimbal_pd_tracker.h
 * @brief Time-gated PD controller for one visual gimbal axis.
 */
#ifndef GIMBAL_PD_TRACKER_H
#define GIMBAL_PD_TRACKER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float kp_pulses_per_pixel;
    float kd_pulse_seconds_per_pixel;
    int16_t deadband_pixels;
    int32_t maximum_pulses;
    uint32_t command_period_ms;
    bool positive_error_is_cw;
} gimbal_pd_tracker_config_t;

typedef struct {
    uint32_t last_command_ms;
    uint32_t previous_update_ms;
    int16_t previous_error_pixels;
    int32_t last_p_term_milli_pulses;
    int32_t last_d_term_milli_pulses;
    int32_t last_output_pulses;
    bool derivative_initialized;
} gimbal_pd_tracker_t;

void gimbal_pd_tracker_init(gimbal_pd_tracker_t *tracker);
bool gimbal_pd_tracker_update(gimbal_pd_tracker_t *tracker,
                              const gimbal_pd_tracker_config_t *config,
                              bool target_valid, int16_t error_pixels,
                              uint32_t now_ms, int32_t *output_pulses);

#endif /* GIMBAL_PD_TRACKER_H */
