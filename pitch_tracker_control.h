#ifndef PITCH_TRACKER_CONTROL_H
#define PITCH_TRACKER_CONTROL_H

#include "pitch_motor_control.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    pitch_motor_t *motor;
    float pulses_per_pixel;
    int16_t deadband_pixels;
    int32_t maximum_pulses;
    uint16_t speed_rpm;
    uint8_t acceleration;
    uint32_t command_period_ms;
    uint32_t last_command_ms;
    bool positive_error_is_cw;
} pitch_tracker_control_t;

void pitch_tracker_control_init(pitch_tracker_control_t *control,
                                pitch_motor_t *motor);
bool pitch_tracker_update(pitch_tracker_control_t *control,
                          bool target_valid, int16_t error_y_pixels,
                          uint32_t now_ms);

#endif /* PITCH_TRACKER_CONTROL_H */
