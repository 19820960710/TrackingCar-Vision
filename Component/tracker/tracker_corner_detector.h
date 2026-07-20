/**
 * @file    tracker_corner_detector.h
 * @brief   Shadow-only spatial feature detector for an 8-channel tracker.
 */

#ifndef TRACKER_CORNER_DETECTOR_H
#define TRACKER_CORNER_DETECTOR_H

#include <stdint.h>

#define TRACKER_CORNER_CHANNEL_COUNT 8u

typedef enum {
    TRACKER_CORNER_FEATURE_NONE = 0,
    TRACKER_CORNER_FEATURE_RIGHT_ANGLE,
    TRACKER_CORNER_FEATURE_ACUTE_ANGLE,
    TRACKER_CORNER_FEATURE_AMBIGUOUS
} tracker_corner_feature_t;

typedef enum {
    TRACKER_CORNER_DIRECTION_NEGATIVE = -1,
    TRACKER_CORNER_DIRECTION_UNKNOWN = 0,
    TRACKER_CORNER_DIRECTION_POSITIVE = 1
} tracker_corner_direction_t;

typedef struct {
    uint16_t strong_line_threshold;
    uint8_t side_active_minimum;
    uint8_t right_angle_confirmation_frames;
    uint8_t acute_angle_confirmation_frames;
    uint8_t rearm_frames;
} tracker_corner_detector_config_t;

typedef struct {
    tracker_corner_detector_config_t config;
    uint32_t update_count;
    uint32_t event_count;
    tracker_corner_feature_t observed_feature;
    tracker_corner_direction_t observed_direction;
    tracker_corner_feature_t candidate_feature;
    tracker_corner_direction_t candidate_direction;
    tracker_corner_feature_t last_feature;
    tracker_corner_direction_t last_direction;
    uint8_t black_mask;
    uint8_t active_run_count;
    uint8_t candidate_frames;
    uint8_t rearm_frames;
    uint8_t event_latched;
} tracker_corner_detector_t;

void tracker_corner_detector_init(
    tracker_corner_detector_t *detector,
    const tracker_corner_detector_config_t *config);

void tracker_corner_detector_reset(
    tracker_corner_detector_t *detector);

/**
 * @brief Inspect one complete black-line sensor frame without controlling motors.
 *
 * @param normalized Per-channel brightness: 0 is calibrated black and
 *                   normalization_max is calibrated white.
 * @param normalization_max Maximum normalized brightness value.
 * @param line_detected Current application-level line-presence decision.
 */
void tracker_corner_detector_update(
    tracker_corner_detector_t *detector,
    const uint16_t normalized[TRACKER_CORNER_CHANNEL_COUNT],
    uint16_t normalization_max,
    uint8_t line_detected);

#endif /* TRACKER_CORNER_DETECTOR_H */

