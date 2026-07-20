#ifndef LINE_TRACKING_CONFIG_H
#define LINE_TRACKING_CONFIG_H

/* The line task owns wheel-speed targets when this switch is enabled. */
#define LINE_TRACKING_ENABLED 1
#define LINE_TRACKING_CONTROL_PERIOD_MS 30u
#define LINE_TRACKING_LINE_IS_WHITE 0u
#define LINE_TRACKING_NORMALIZATION_MAX 1000u
#define LINE_TRACKING_PEAK_ENTER_THRESHOLD 80u
#define LINE_TRACKING_PEAK_EXIT_THRESHOLD 60u

/* Encoder geometry used by the imported corner alignment. */
#define LINE_TRACKING_ENCODER_COUNTS_PER_REV 2464.0f
#define LINE_TRACKING_WHEEL_RADIUS_MM 32.0f

/* Loss fallback: clockwise in-place 90 degree turn, then check the line. */
/* Verified source-project convention: left forward/right reverse turns right. */
#define LINE_TRACKING_LOSS_TURN_LEFT_MM_S 200.0f
#define LINE_TRACKING_LOSS_TURN_RIGHT_MM_S -200.0f
#define LINE_TRACKING_LOSS_TURN_90_COUNTS 1650
#define LINE_TRACKING_LOSS_TURN_TIMEOUT_CYCLES 100u
#define LINE_TRACKING_LOSS_REACQUIRE_CYCLES 2u

/* Calibration measured in Gear_motor_release_20260720. */
#define LINE_TRACKING_BLACK_CALIBRATION \
    {2012u, 1807u, 606u, 688u, 436u, 966u, 760u, 493u}
#define LINE_TRACKING_WHITE_CALIBRATION \
    {3171u, 3157u, 3087u, 3069u, 2989u, 3105u, 3099u, 3040u}

/* Active 200 mm/s candidate247 from the source project. */
#define LINE_TRACKING_POSITION_CONFIG                                  \
    {                                                                  \
        .kp = 110.0f,                                                   \
        .ki = 0.0f,                                                     \
        .kd = 2.5f,                                                     \
        .edge_gain = 96.3f,                                             \
        .integral_limit = 0.0f,                                         \
        .correction_limit = 292.2f,                                     \
        .base_speed = 200.0f,                                           \
        .min_target_speed = 0.0f,                                       \
        .max_target_speed = 1000.0f,                                    \
        .derivative_filter_time_constant_s = 0.050f,                    \
        .error_prediction_time_s = 0.060f,                              \
        .correction_slew_limit_per_step = 14.0f,                        \
        .error_slowdown_gain = 239.4f,                                  \
        .derivative_slowdown_gain = 8.45f,                              \
        .minimum_effective_base_speed = 129.8f,                         \
        .effective_base_slew_limit_per_step = 22.8f,                    \
    }

#define LINE_TRACKING_CORNER_DETECTOR_CONFIG                            \
    {                                                                  \
        .strong_line_threshold = 650u,                                 \
        .side_active_minimum = 3u,                                     \
        .right_angle_confirmation_frames = 2u,                         \
        .acute_angle_confirmation_frames = 2u,                         \
        .rearm_frames = 3u,                                            \
    }

/* Imported right-angle alignment trial; acute-angle execution stays off. */
#define LINE_TRACKING_CORNER_ALIGNMENT_CONFIG                           \
    {                                                                  \
        .candidate_target_speed_mm_s = 80.0f,                          \
        .forward_target_speed_mm_s = 100.0f,                           \
        .max_probe_distance_mm = 30.0f,                                \
        .backoff_distance_mm = 10.0f,                                  \
        .backoff_target_speed_mm_s = 100.0f,                           \
        .forced_turn_direction = TRACKER_CORNER_DIRECTION_POSITIVE,    \
        .candidate_release_frames = 2u,                                \
        .line_loss_acute_fallback_enable = 0u,                         \
        .candidate_min_hold_cycles = 4u,                               \
        .candidate_timeout_cycles = 6u,                                \
        .probe_timeout_cycles = 20u,                                   \
        .backoff_timeout_cycles = 50u,                                 \
        .right_angle_inner_distance_mm = 83.25221f,                    \
        .right_angle_outer_distance_mm = 356.57077f,                   \
        .acute_angle_inner_distance_mm = -48.66069f,                   \
        .acute_angle_outer_distance_mm = 302.41229f,                   \
        .turn_ready_cycles = 20u,                                      \
        .turn_timeout_cycles = 450u,                                   \
        .turn_complete_cycles = 30u,                                   \
        .post_turn_scan_track_width_mm = 174.0f,                       \
        .post_turn_scan_offset_deg = 5.0f,                             \
        .release_centered_error_limit = 0.30f,                         \
        .post_turn_scan_line_threshold =                               \
            LINE_TRACKING_PEAK_ENTER_THRESHOLD,                        \
        .release_stable_cycles = 5u,                                   \
        .post_turn_scan_enable = 1u,                                   \
        .right_angle_direct = {                                        \
            .enable = 1u,                                              \
            .candidate_target_speed_mm_s = 200.0f,                     \
            .outer_max_speed_mm_s = 400.0f,                            \
            .target_slew_limit_per_step = 30.0f,                       \
            .minimum_alignment_progress = 0.60f,                       \
            .alignment_base_speed_mm_s = 200.0f,                       \
            .alignment_kp = 100.0f,                                    \
            .alignment_correction_limit = 100.0f,                      \
            .alignment_correction_slew_limit_per_step = 30.0f,         \
            .release_error_delta_limit = 0.08f,                        \
            .release_stable_cycles = 3u,                               \
            .alignment_line_loss_timeout_cycles = 20u,                 \
        },                                                             \
        .wheel_angle_config = {                                        \
            .kp = 6.0f,                                                \
            .wheel_radius_mm = LINE_TRACKING_WHEEL_RADIUS_MM,          \
            .max_angular_speed_rad_s = 1.5f,                           \
            .angle_tolerance_rad =                                     \
                0.55f * (6.283185307f /                                \
                         LINE_TRACKING_ENCODER_COUNTS_PER_REV),         \
            .stop_speed_tolerance_rad_s = 0.2f,                        \
            .settle_cycles = 5u,                                       \
        },                                                             \
        .acute_execution_enable = 0u,                                  \
    }

#endif
