/**
 * @file gimbal_pd_tracker.c
 * @brief Hardware-independent visual PD controller migrated from tracking_vision.
 */
#include "control/gimbal_pd_tracker.h"

#include <stddef.h>

static int32_t clamp_i32(int32_t value, int32_t maximum)
{
    if (value > maximum) {
        return maximum;
    }
    if (value < -maximum) {
        return -maximum;
    }
    return value;
}

static void reset_control_terms(gimbal_pd_tracker_t *tracker,
                                int32_t *output_pulses)
{
    tracker->derivative_initialized = false;
    tracker->last_p_term_milli_pulses = 0;
    tracker->last_d_term_milli_pulses = 0;
    tracker->last_output_pulses = 0;
    *output_pulses = 0;
}

void gimbal_pd_tracker_init(gimbal_pd_tracker_t *tracker)
{
    if (tracker != NULL) {
        *tracker = (gimbal_pd_tracker_t){0};
    }
}

gimbal_pd_action_t gimbal_pd_tracker_update(
    gimbal_pd_tracker_t *tracker,
    const gimbal_pd_tracker_config_t *config,
    bool target_valid, int16_t error_pixels,
    uint32_t now_ms, int32_t *output_pulses)
{
    float p_term;
    float d_term = 0.0f;
    int32_t pulses;
    uint32_t elapsed_ms;

    if ((tracker == NULL) || (config == NULL) || (output_pulses == NULL)) {
        return GIMBAL_PD_ACTION_HOLD;
    }
    if (!target_valid) {
        tracker->center_hold_active = true;
        reset_control_terms(tracker, output_pulses);
        return GIMBAL_PD_ACTION_STOP;
    }
    if (tracker->center_hold_active) {
        if ((error_pixels <= config->restart_band_pixels) &&
            (error_pixels >= -config->restart_band_pixels)) {
            reset_control_terms(tracker, output_pulses);
            return GIMBAL_PD_ACTION_STOP;
        }
        tracker->center_hold_active = false;
        tracker->derivative_initialized = false;
    }
    if ((error_pixels <= config->stop_band_pixels) &&
        (error_pixels >= -config->stop_band_pixels)) {
        tracker->center_hold_active = true;
        reset_control_terms(tracker, output_pulses);
        return GIMBAL_PD_ACTION_STOP;
    }
    if ((uint32_t)(now_ms - tracker->last_command_ms) <
        config->command_period_ms) {
        return GIMBAL_PD_ACTION_HOLD;
    }

    p_term = (float)error_pixels * config->kp_pulses_per_pixel;
    elapsed_ms = (uint32_t)(now_ms - tracker->previous_update_ms);
    if (tracker->derivative_initialized && (elapsed_ms != 0U)) {
        d_term = config->kd_pulse_seconds_per_pixel *
                 (float)(error_pixels - tracker->previous_error_pixels) *
                 (1000.0f / (float)elapsed_ms);
    }
    pulses = (int32_t)(p_term + d_term);
    if (pulses == 0) {
        pulses = (error_pixels > 0) ? 1 : -1;
    }
    pulses = clamp_i32(pulses, config->maximum_pulses);
    if (!config->positive_error_is_cw) {
        pulses = -pulses;
    }

    tracker->last_command_ms = now_ms;
    tracker->previous_update_ms = now_ms;
    tracker->previous_error_pixels = error_pixels;
    tracker->last_p_term_milli_pulses = (int32_t)(p_term * 1000.0f);
    tracker->last_d_term_milli_pulses = (int32_t)(d_term * 1000.0f);
    tracker->last_output_pulses = pulses;
    tracker->derivative_initialized = true;
    *output_pulses = pulses;
    return GIMBAL_PD_ACTION_MOVE;
}
