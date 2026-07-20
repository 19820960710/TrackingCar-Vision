#include "tracker_corner_detector.h"

#include <stddef.h>

#define TRACKER_CORNER_CENTER_MASK      0x18u
#define TRACKER_CORNER_CENTER_ZONE_MASK 0x3Cu
#define TRACKER_CORNER_NEGATIVE_HALF    0x0Fu
#define TRACKER_CORNER_POSITIVE_HALF    0xF0u
#define TRACKER_CORNER_NEGATIVE_OUTER   0x07u
#define TRACKER_CORNER_POSITIVE_OUTER   0xE0u

static uint8_t tracker_corner_count_active(uint8_t mask)
{
    uint8_t count = 0u;

    while (mask != 0u) {
        count += (uint8_t)(mask & 1u);
        mask >>= 1u;
    }

    return count;
}

static uint8_t tracker_corner_count_runs(uint8_t mask)
{
    uint8_t run_count = 0u;
    uint8_t previous_active = 0u;
    uint8_t i;

    for (i = 0u; i < TRACKER_CORNER_CHANNEL_COUNT; i++) {
        uint8_t active = (uint8_t)((mask >> i) & 1u);

        if ((active != 0u) && (previous_active == 0u)) {
            run_count++;
        }
        previous_active = active;
    }

    return run_count;
}

static uint8_t tracker_corner_build_black_mask(
    const tracker_corner_detector_t *detector,
    const uint16_t normalized[TRACKER_CORNER_CHANNEL_COUNT],
    uint16_t normalization_max)
{
    uint8_t mask = 0u;
    uint8_t i;

    for (i = 0u; i < TRACKER_CORNER_CHANNEL_COUNT; i++) {
        uint16_t brightness = normalized[i];
        uint16_t black_strength;

        if (brightness > normalization_max) {
            brightness = normalization_max;
        }
        black_strength = (uint16_t)(normalization_max - brightness);
        if (black_strength >= detector->config.strong_line_threshold) {
            mask |= (uint8_t)(1u << i);
        }
    }

    return mask;
}

static tracker_corner_feature_t tracker_corner_classify(
    const tracker_corner_detector_t *detector,
    uint8_t mask,
    uint8_t active_run_count,
    tracker_corner_direction_t *direction)
{
    uint8_t negative_outer_active;
    uint8_t positive_outer_active;
    uint8_t negative_half_active;
    uint8_t positive_half_active;

    *direction = TRACKER_CORNER_DIRECTION_UNKNOWN;
    if ((mask & TRACKER_CORNER_CENTER_MASK) == 0u) {
        return TRACKER_CORNER_FEATURE_NONE;
    }

    negative_outer_active =
        (mask & TRACKER_CORNER_NEGATIVE_OUTER) != 0u;
    positive_outer_active =
        (mask & TRACKER_CORNER_POSITIVE_OUTER) != 0u;
    if ((negative_outer_active != 0u) &&
        (positive_outer_active != 0u)) {
        return TRACKER_CORNER_FEATURE_AMBIGUOUS;
    }

    negative_half_active = tracker_corner_count_active(
        (uint8_t)(mask & TRACKER_CORNER_NEGATIVE_HALF));
    positive_half_active = tracker_corner_count_active(
        (uint8_t)(mask & TRACKER_CORNER_POSITIVE_HALF));

    if (active_run_count == 1u) {
        if ((negative_half_active >=
             detector->config.side_active_minimum) &&
            (positive_half_active <
             detector->config.side_active_minimum)) {
            *direction = TRACKER_CORNER_DIRECTION_NEGATIVE;
            return TRACKER_CORNER_FEATURE_RIGHT_ANGLE;
        }
        if ((positive_half_active >=
             detector->config.side_active_minimum) &&
            (negative_half_active <
             detector->config.side_active_minimum)) {
            *direction = TRACKER_CORNER_DIRECTION_POSITIVE;
            return TRACKER_CORNER_FEATURE_RIGHT_ANGLE;
        }
    }

    if (active_run_count >= 2u) {
        if ((negative_outer_active != 0u) &&
            (positive_outer_active == 0u)) {
            *direction = TRACKER_CORNER_DIRECTION_NEGATIVE;
            return TRACKER_CORNER_FEATURE_ACUTE_ANGLE;
        }
        if ((positive_outer_active != 0u) &&
            (negative_outer_active == 0u)) {
            *direction = TRACKER_CORNER_DIRECTION_POSITIVE;
            return TRACKER_CORNER_FEATURE_ACUTE_ANGLE;
        }
    }

    return TRACKER_CORNER_FEATURE_NONE;
}

static void tracker_corner_clear_candidate(
    tracker_corner_detector_t *detector)
{
    detector->candidate_feature = TRACKER_CORNER_FEATURE_NONE;
    detector->candidate_direction = TRACKER_CORNER_DIRECTION_UNKNOWN;
    detector->candidate_frames = 0u;
}

static uint8_t tracker_corner_is_centered_tracking(
    uint8_t mask,
    uint8_t active_run_count)
{
    return (uint8_t)(
        (active_run_count == 1u) &&
        ((mask & TRACKER_CORNER_CENTER_MASK) != 0u) &&
        ((mask & (uint8_t)~TRACKER_CORNER_CENTER_ZONE_MASK) == 0u));
}

static uint8_t tracker_corner_required_confirmation_frames(
    const tracker_corner_detector_t *detector,
    tracker_corner_feature_t feature)
{
    if (feature == TRACKER_CORNER_FEATURE_ACUTE_ANGLE) {
        return detector->config.acute_angle_confirmation_frames;
    }

    return detector->config.right_angle_confirmation_frames;
}

static uint8_t tracker_corner_acute_followup_matches(
    uint8_t mask,
    tracker_corner_direction_t direction)
{
    uint8_t side_outer_mask;

    if ((mask & TRACKER_CORNER_CENTER_MASK) == 0u) {
        return 0u;
    }

    if (direction == TRACKER_CORNER_DIRECTION_NEGATIVE) {
        side_outer_mask = TRACKER_CORNER_NEGATIVE_OUTER;
    } else if (direction == TRACKER_CORNER_DIRECTION_POSITIVE) {
        side_outer_mask = TRACKER_CORNER_POSITIVE_OUTER;
    } else {
        return 0u;
    }

    return (uint8_t)(tracker_corner_count_active(
        (uint8_t)(mask & side_outer_mask)) >= 2u);
}

void tracker_corner_detector_reset(
    tracker_corner_detector_t *detector)
{
    if (detector == NULL) {
        return;
    }

    detector->update_count = 0u;
    detector->event_count = 0u;
    detector->observed_feature = TRACKER_CORNER_FEATURE_NONE;
    detector->observed_direction = TRACKER_CORNER_DIRECTION_UNKNOWN;
    tracker_corner_clear_candidate(detector);
    detector->last_feature = TRACKER_CORNER_FEATURE_NONE;
    detector->last_direction = TRACKER_CORNER_DIRECTION_UNKNOWN;
    detector->black_mask = 0u;
    detector->active_run_count = 0u;
    detector->rearm_frames = 0u;
    detector->event_latched = 0u;
}

void tracker_corner_detector_init(
    tracker_corner_detector_t *detector,
    const tracker_corner_detector_config_t *config)
{
    if ((detector == NULL) || (config == NULL)) {
        return;
    }

    detector->config = *config;
    tracker_corner_detector_reset(detector);
}

void tracker_corner_detector_update(
    tracker_corner_detector_t *detector,
    const uint16_t normalized[TRACKER_CORNER_CHANNEL_COUNT],
    uint16_t normalization_max,
    uint8_t line_detected)
{
    tracker_corner_feature_t observed_feature;
    tracker_corner_direction_t observed_direction;

    if ((detector == NULL) || (normalized == NULL) ||
        (normalization_max == 0u)) {
        return;
    }

    detector->update_count++;
    detector->black_mask = 0u;
    detector->active_run_count = 0u;
    detector->observed_feature = TRACKER_CORNER_FEATURE_NONE;
    detector->observed_direction = TRACKER_CORNER_DIRECTION_UNKNOWN;

    if (line_detected == 0u) {
        tracker_corner_clear_candidate(detector);
        detector->rearm_frames = 0u;
        return;
    }

    detector->black_mask = tracker_corner_build_black_mask(
        detector, normalized, normalization_max);
    detector->active_run_count = tracker_corner_count_runs(
        detector->black_mask);
    observed_feature = tracker_corner_classify(
        detector,
        detector->black_mask,
        detector->active_run_count,
        &observed_direction);
    detector->observed_feature = observed_feature;
    detector->observed_direction = observed_direction;

    if (detector->event_latched != 0u) {
        tracker_corner_clear_candidate(detector);
        if (tracker_corner_is_centered_tracking(
                detector->black_mask,
                detector->active_run_count) != 0u) {
            if (detector->rearm_frames < UINT8_MAX) {
                detector->rearm_frames++;
            }
            if (detector->rearm_frames >=
                detector->config.rearm_frames) {
                detector->event_latched = 0u;
                detector->rearm_frames = 0u;
            }
        } else {
            detector->rearm_frames = 0u;
        }
        return;
    }

    detector->rearm_frames = 0u;

    if ((detector->candidate_feature ==
         TRACKER_CORNER_FEATURE_ACUTE_ANGLE) &&
        (tracker_corner_acute_followup_matches(
             detector->black_mask,
             detector->candidate_direction) != 0u)) {
        if (detector->candidate_frames < UINT8_MAX) {
            detector->candidate_frames++;
        }
    } else if ((observed_feature != TRACKER_CORNER_FEATURE_RIGHT_ANGLE) &&
               (observed_feature != TRACKER_CORNER_FEATURE_ACUTE_ANGLE)) {
        tracker_corner_clear_candidate(detector);
        return;
    } else if ((observed_feature != detector->candidate_feature) ||
               (observed_direction != detector->candidate_direction)) {
        detector->candidate_feature = observed_feature;
        detector->candidate_direction = observed_direction;
        detector->candidate_frames = 1u;
    } else if (detector->candidate_frames < UINT8_MAX) {
        detector->candidate_frames++;
    }

    if (detector->candidate_frames >=
        tracker_corner_required_confirmation_frames(
            detector, detector->candidate_feature)) {
        detector->event_latched = 1u;
        detector->last_feature = detector->candidate_feature;
        detector->last_direction = detector->candidate_direction;
        detector->event_count++;
    }
}

