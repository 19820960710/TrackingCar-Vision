#include "pitch_tracker_control.h"

#include <limits.h>

#define PITCH_TRACKER_DEFAULT_KP_PULSES_PER_PIXEL (0.25f)
#define PITCH_TRACKER_DEFAULT_KD_PULSE_SECONDS_PER_PIXEL (0.0f)
#define PITCH_TRACKER_DEFAULT_DEADBAND_PIXELS   (8)
#define PITCH_TRACKER_DEFAULT_MAXIMUM_PULSES    (30)
#define PITCH_TRACKER_DEFAULT_SPEED_RPM         (60U)
#define PITCH_TRACKER_DEFAULT_ACCELERATION      (20U)
#define PITCH_TRACKER_DEFAULT_PERIOD_MS         (200U)

void pitch_tracker_control_init(pitch_tracker_control_t *control,
                                pitch_motor_t *motor)
{
    control->motor = motor;
    control->kp_pulses_per_pixel =
        PITCH_TRACKER_DEFAULT_KP_PULSES_PER_PIXEL;
    control->kd_pulse_seconds_per_pixel =
        PITCH_TRACKER_DEFAULT_KD_PULSE_SECONDS_PER_PIXEL;
    control->deadband_pixels = PITCH_TRACKER_DEFAULT_DEADBAND_PIXELS;
    control->maximum_pulses = PITCH_TRACKER_DEFAULT_MAXIMUM_PULSES;
    control->speed_rpm = PITCH_TRACKER_DEFAULT_SPEED_RPM;
    control->acceleration = PITCH_TRACKER_DEFAULT_ACCELERATION;
    control->command_period_ms = PITCH_TRACKER_DEFAULT_PERIOD_MS;
    control->last_command_ms = 0U;
    control->previous_update_ms = 0U;
    control->previous_error_pixels = 0;
    control->last_p_term_milli_pulses = 0;
    control->last_d_term_milli_pulses = 0;
    control->last_output_pulses = 0;
    control->positive_error_is_cw = true;
    control->motion_command_active = false;
    control->derivative_initialized = false;
}

bool pitch_tracker_update(pitch_tracker_control_t *control,
                          bool target_valid, int16_t error_y_pixels,
                          uint32_t now_ms)
{
    float p_term;
    float d_term = 0.0f;
    float output;
    uint32_t elapsed_ms;
    int32_t pulses;

    if (control->motor == NULL) {
        return false;
    }

    if ((!target_valid) ||
        (error_y_pixels <= control->deadband_pixels &&
         error_y_pixels >= -control->deadband_pixels)) {
        bool stop_submitted = false;

        if (control->motion_command_active) {
            if (!pitch_motor_stop(control->motor)) {
                return false;
            }
            stop_submitted = true;
        }

        control->motion_command_active = false;
        control->derivative_initialized = false;
        control->last_p_term_milli_pulses = 0;
        control->last_d_term_milli_pulses = 0;
        control->last_output_pulses = 0;
        if (!stop_submitted) {
            return false;
        }

        control->last_command_ms = now_ms;
        return true;
    }

    if ((uint32_t) (now_ms - control->last_command_ms) <
        control->command_period_ms) {
        return false;
    }

    p_term = (float) error_y_pixels * control->kp_pulses_per_pixel;
    elapsed_ms = (uint32_t) (now_ms - control->previous_update_ms);
    if (control->derivative_initialized && (elapsed_ms != 0U)) {
        d_term = control->kd_pulse_seconds_per_pixel *
            (float) (error_y_pixels - control->previous_error_pixels) *
            (1000.0f / (float) elapsed_ms);
    }
    output = p_term + d_term;
    pulses = (int32_t) output;
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
    control->previous_update_ms = now_ms;
    control->previous_error_pixels = error_y_pixels;
    control->last_p_term_milli_pulses = (int32_t) (p_term * 1000.0f);
    control->last_d_term_milli_pulses = (int32_t) (d_term * 1000.0f);
    control->last_output_pulses = pulses;
    control->motion_command_active = true;
    control->derivative_initialized = true;
    return true;
}

void pitch_tracker_prepare_response(pitch_tracker_control_t *control,
                                    int16_t error_pixels, uint32_t now_ms)
{
    if (control == NULL) {
        return;
    }

    control->previous_error_pixels = error_pixels;
    control->previous_update_ms = now_ms;
    control->last_command_ms = now_ms - control->command_period_ms;
    control->last_p_term_milli_pulses = 0;
    control->last_d_term_milli_pulses = 0;
    control->last_output_pulses = 0;
    control->derivative_initialized = true;
    control->motion_command_active = false;
}

void pitch_tracker_get_diagnostics(const pitch_tracker_control_t *control,
                                   pitch_tracker_diagnostics_t *out)
{
    if ((control == NULL) || (out == NULL)) {
        return;
    }

    out->p_term_milli_pulses = control->last_p_term_milli_pulses;
    out->d_term_milli_pulses = control->last_d_term_milli_pulses;
    out->output_pulses = control->last_output_pulses;
}
