#include "pitch_tracker_control.h"

#include <limits.h>

#define PITCH_TRACKER_DEFAULT_PULSES_PER_PIXEL  (0.25f)
#define PITCH_TRACKER_DEFAULT_DEADBAND_PIXELS   (8)
#define PITCH_TRACKER_DEFAULT_MAXIMUM_PULSES    (30)
#define PITCH_TRACKER_DEFAULT_SPEED_RPM         (60U)
#define PITCH_TRACKER_DEFAULT_ACCELERATION      (20U)
#define PITCH_TRACKER_DEFAULT_PERIOD_MS         (200U)

void pitch_tracker_control_init(pitch_tracker_control_t *control,
                                pitch_motor_t *motor)
{
    control->motor = motor;
    control->pulses_per_pixel = PITCH_TRACKER_DEFAULT_PULSES_PER_PIXEL;
    control->deadband_pixels = PITCH_TRACKER_DEFAULT_DEADBAND_PIXELS;
    control->maximum_pulses = PITCH_TRACKER_DEFAULT_MAXIMUM_PULSES;
    control->speed_rpm = PITCH_TRACKER_DEFAULT_SPEED_RPM;
    control->acceleration = PITCH_TRACKER_DEFAULT_ACCELERATION;
    control->command_period_ms = PITCH_TRACKER_DEFAULT_PERIOD_MS;
    control->last_command_ms = 0U;
    control->positive_error_is_cw = true;
}

bool pitch_tracker_update(pitch_tracker_control_t *control,
                          bool target_valid, int16_t error_y_pixels,
                          uint32_t now_ms)
{
    int32_t pulses;

    if ((!target_valid) || (control->motor == NULL) ||
        (error_y_pixels <= control->deadband_pixels &&
         error_y_pixels >= -control->deadband_pixels) ||
        ((uint32_t) (now_ms - control->last_command_ms) <
         control->command_period_ms)) {
        return false;
    }

    pulses = (int32_t) ((float) error_y_pixels * control->pulses_per_pixel);
    if (pulses == 0) {
        pulses = (error_y_pixels > 0) ? 1 : -1;
    }
    if (pulses > control->maximum_pulses) {
        pulses = control->maximum_pulses;
    } else if (pulses < -control->maximum_pulses) {
        pulses = -control->maximum_pulses;
    }
    if (!control->positive_error_is_cw) {
        pulses = -pulses;
    }

    if (!pitch_motor_move_relative(control->motor, pulses, control->speed_rpm,
                                   control->acceleration)) {
        return false;
    }

    control->last_command_ms = now_ms;
    return true;
}
