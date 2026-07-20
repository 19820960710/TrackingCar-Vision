/**
 * @file    tracker_corner_alignment.h
 * @brief   Encoder-bounded corner alignment and two-wheel turn execution.
 */

#ifndef TRACKER_CORNER_ALIGNMENT_H
#define TRACKER_CORNER_ALIGNMENT_H

#include <stdint.h>

#include "gear_motor_angle_control.h"
#include "tracker_corner_detector.h"

#define TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT 2u
#define TRACKER_CORNER_ALIGNMENT_LEFT_WHEEL  0u
#define TRACKER_CORNER_ALIGNMENT_RIGHT_WHEEL 1u

typedef enum {
    TRACKER_CORNER_ALIGNMENT_WAIT_EVENT = 0,
    TRACKER_CORNER_ALIGNMENT_FORWARD_PROBE,
    TRACKER_CORNER_ALIGNMENT_BACKOFF,
    TRACKER_CORNER_ALIGNMENT_TURN_READY,
    TRACKER_CORNER_ALIGNMENT_ABORT,
    TRACKER_CORNER_ALIGNMENT_CANDIDATE_HOLD,
    TRACKER_CORNER_ALIGNMENT_TURNING,
    TRACKER_CORNER_ALIGNMENT_RIGHT_ANGLE_ALIGNING,
    TRACKER_CORNER_ALIGNMENT_POST_TURN_SCAN,
    TRACKER_CORNER_ALIGNMENT_TURN_COMPLETE,
    TRACKER_CORNER_ALIGNMENT_TRACKING_RELEASE
} tracker_corner_alignment_mode_t;

typedef enum {
    TRACKER_CORNER_SCAN_IDLE = 0,
    TRACKER_CORNER_SCAN_TO_50_DEG,
    TRACKER_CORNER_SCAN_TO_40_DEG,
    TRACKER_CORNER_SCAN_RETURN_45_DEG
} tracker_corner_alignment_scan_phase_t;

typedef enum {
    TRACKER_CORNER_ALIGNMENT_ABORT_NONE = 0,
    TRACKER_CORNER_ALIGNMENT_ABORT_PROBE_DISTANCE,
    TRACKER_CORNER_ALIGNMENT_ABORT_PROBE_TIMEOUT,
    TRACKER_CORNER_ALIGNMENT_ABORT_BACKOFF_TIMEOUT,
    TRACKER_CORNER_ALIGNMENT_ABORT_CANDIDATE_TIMEOUT,
    TRACKER_CORNER_ALIGNMENT_ABORT_CANDIDATE_LINE_LOSS,
    TRACKER_CORNER_ALIGNMENT_ABORT_TURN_START,
    TRACKER_CORNER_ALIGNMENT_ABORT_TURN_TIMEOUT
} tracker_corner_alignment_abort_t;

typedef enum {
    TRACKER_CORNER_DIAGNOSTIC_NONE = 0,
    TRACKER_CORNER_DIAGNOSTIC_CANDIDATE_UNCONFIRMED = 2,
    TRACKER_CORNER_DIAGNOSTIC_CANDIDATE_LINE_LOSS = 3,
    TRACKER_CORNER_DIAGNOSTIC_PROBE_FAULT = 4,
    TRACKER_CORNER_DIAGNOSTIC_BACKOFF_FAULT = 5,
    TRACKER_CORNER_DIAGNOSTIC_TURN_FAULT = 6
} tracker_corner_diagnostic_t;

typedef struct {
    uint8_t enable;
    float candidate_target_speed_mm_s;
    float outer_max_speed_mm_s;
    float target_slew_limit_per_step;
    float minimum_alignment_progress;
    float alignment_base_speed_mm_s;
    float alignment_kp;
    float alignment_correction_limit;
    float alignment_correction_slew_limit_per_step;
    float release_error_delta_limit;
    uint16_t release_stable_cycles;
    uint16_t alignment_line_loss_timeout_cycles;
} tracker_corner_right_angle_direct_config_t;

typedef struct {
    float candidate_target_speed_mm_s;
    float forward_target_speed_mm_s;
    float max_probe_distance_mm;
    float backoff_distance_mm;
    float backoff_target_speed_mm_s;
    tracker_corner_direction_t forced_turn_direction;
    uint8_t candidate_release_frames;
    uint8_t line_loss_acute_fallback_enable;
    uint16_t candidate_min_hold_cycles;
    uint16_t candidate_timeout_cycles;
    uint16_t probe_timeout_cycles;
    uint16_t backoff_timeout_cycles;
    float right_angle_inner_distance_mm;
    float right_angle_outer_distance_mm;
    float acute_angle_inner_distance_mm;
    float acute_angle_outer_distance_mm;
    uint16_t turn_ready_cycles;
    uint16_t turn_timeout_cycles;
    uint16_t turn_complete_cycles;
    float post_turn_scan_track_width_mm;
    float post_turn_scan_offset_deg;
    float release_centered_error_limit;
    uint16_t post_turn_scan_line_threshold;
    uint16_t release_stable_cycles;
    uint8_t post_turn_scan_enable;
    tracker_corner_right_angle_direct_config_t right_angle_direct;
    gear_motor_angle_config_t wheel_angle_config;
    uint8_t acute_execution_enable;
} tracker_corner_alignment_config_t;

typedef struct {
    uint32_t detector_event_count;
    tracker_corner_feature_t detected_feature;
    tracker_corner_direction_t detected_direction;
    tracker_corner_feature_t candidate_feature;
    tracker_corner_direction_t candidate_direction;
    uint8_t candidate_frames;
    uint8_t line_sample_valid;
    uint8_t line_detected;
    uint8_t scan_center_line_detected;
    float line_error;
    float wheel_angle_rad[TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT];
    float measured_speed_mm_s[TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT];
} tracker_corner_alignment_input_t;

typedef struct {
    float left_target_speed_mm_s;
    float right_target_speed_mm_s;
    tracker_corner_alignment_mode_t mode;
    tracker_corner_alignment_abort_t abort_reason;
    tracker_corner_diagnostic_t diagnostic_result;
    uint8_t diagnostic_latched;
    uint8_t overriding;
} tracker_corner_alignment_output_t;

typedef struct {
    tracker_corner_alignment_config_t config;
    gear_motor_angle_controller_t
        wheel_controller[TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT];
    tracker_corner_alignment_mode_t mode;
    tracker_corner_alignment_abort_t abort_reason;
    tracker_corner_diagnostic_t diagnostic_result;
    tracker_corner_feature_t feature;
    tracker_corner_direction_t direction;
    tracker_corner_alignment_scan_phase_t scan_phase;
    float probe_start_angle_rad[TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT];
    float line_loss_angle_rad[TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT];
    float probe_distance_mm[TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT];
    float backoff_progress_mm[TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT];
    float turn_distance_mm[TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT];
    float target_speed_mm_s[TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT];
    uint32_t last_consumed_event_count;
    uint32_t update_count;
    uint32_t activation_count;
    uint32_t candidate_attempt_count;
    uint32_t candidate_release_count;
    uint32_t turn_count;
    uint32_t turn_complete_count;
    uint32_t scan_count;
    uint32_t scan_line_found_count;
    uint32_t abort_count;
    uint16_t elapsed_cycles;
    uint16_t backoff_settle_cycles;
    uint16_t release_centered_cycles;
    uint16_t right_angle_line_loss_cycles;
    uint8_t candidate_reject_frames;
    uint8_t backoff_reached[TRACKER_CORNER_ALIGNMENT_WHEEL_COUNT];
    uint8_t backoff_line_reacquired;
    uint8_t diagnostic_latched;
    uint8_t right_angle_has_last_error;
    float right_angle_alignment_correction;
    float right_angle_last_error;
    uint8_t initialized;
} tracker_corner_alignment_t;

/** @return Configuration is valid and initialization succeeded. */
uint8_t tracker_corner_alignment_init(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_config_t *config,
    uint32_t initial_event_count);

/**
 * @brief Release motor ownership and wait for events newer than event_count.
 *
 * @note Lifetime activation and abort counters are retained for debugging.
 */
void tracker_corner_alignment_reset(
    tracker_corner_alignment_t *controller,
    uint32_t event_count);

/** @brief Run one 30 ms outer-loop update. */
void tracker_corner_alignment_update(
    tracker_corner_alignment_t *controller,
    const tracker_corner_alignment_input_t *input,
    tracker_corner_alignment_output_t *output);

/** @return Nonzero while alignment must own both motor targets. */
uint8_t tracker_corner_alignment_is_overriding(
    const tracker_corner_alignment_t *controller);

/**
 * @brief Detect a black line on zero-based tracker channels 2 through 5.
 */
uint8_t tracker_corner_alignment_scan_center_line_detected(
    const uint16_t normalized[TRACKER_CORNER_CHANNEL_COUNT],
    uint16_t normalization_max,
    uint16_t line_threshold);

#endif /* TRACKER_CORNER_ALIGNMENT_H */

