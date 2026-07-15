#ifndef PITCH_TRACKER_CONTROL_H
#define PITCH_TRACKER_CONTROL_H

#include "pitch_motor_control.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    pitch_motor_t *motor;
    float kp_pulses_per_pixel;
    float kd_pulse_seconds_per_pixel;
    int16_t deadband_pixels;
    int32_t maximum_pulses;
    uint16_t speed_rpm;
    uint8_t acceleration;
    uint32_t command_period_ms;
    uint32_t last_command_ms;
    uint32_t previous_update_ms;
    int16_t previous_error_pixels;
    int32_t last_p_term_milli_pulses;
    int32_t last_d_term_milli_pulses;
    int32_t last_output_pulses;
    bool positive_error_is_cw;
    bool motion_command_active;
    bool derivative_initialized;
} pitch_tracker_control_t;

typedef struct {
    int32_t p_term_milli_pulses;
    int32_t d_term_milli_pulses;
    int32_t output_pulses;
} pitch_tracker_diagnostics_t;

void pitch_tracker_control_init(pitch_tracker_control_t *control,
                                pitch_motor_t *motor);
bool pitch_tracker_update(pitch_tracker_control_t *control,
                          bool target_valid, int16_t error_y_pixels,
                          uint32_t now_ms);
void pitch_tracker_prepare_response(pitch_tracker_control_t *control,
                                    int16_t error_pixels, uint32_t now_ms);
void pitch_tracker_get_diagnostics(const pitch_tracker_control_t *control,
                                   pitch_tracker_diagnostics_t *out);

#endif /* PITCH_TRACKER_CONTROL_H */
