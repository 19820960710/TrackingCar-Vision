/**
 * @file vision_stimulus.c
 * @brief Finite-rate synthetic pixel targets, not a camera feedback model.
 */
#include "vision/vision_stimulus.h"

#include "config/vision_tracking_config.h"

#include <stddef.h>

#define VISION_STIMULUS_FRAME_WIDTH       320U
#define VISION_STIMULUS_FRAME_HEIGHT      240U
#define VISION_STIMULUS_CENTER_X          (VISION_STIMULUS_FRAME_WIDTH / 2U)
#define VISION_STIMULUS_CENTER_Y          (VISION_STIMULUS_FRAME_HEIGHT / 2U)

static uint32_t next_random(vision_stimulus_t *stimulus)
{
    stimulus->random_state = stimulus->random_state * 1664525U + 1013904223U;
    return stimulus->random_state;
}

static int16_t next_offset(vision_stimulus_t *stimulus, int16_t maximum)
{
    uint32_t span = (uint32_t)(2 * maximum + 1);

    return (int16_t)(next_random(stimulus) % span) - maximum;
}

void vision_stimulus_init(vision_stimulus_t *stimulus, uint32_t seed)
{
    if (stimulus != NULL) {
        stimulus->next_sample_ms = 0U;
        stimulus->sequence = 0U;
        stimulus->random_state = (seed == 0U) ? 1U : seed;
    }
}

bool vision_stimulus_take(vision_stimulus_t *stimulus, uint32_t now_ms,
                          vision_observation_t *out)
{
    int16_t dx;
    int16_t dy;

    if ((stimulus == NULL) || (out == NULL) ||
        ((int32_t)(now_ms - stimulus->next_sample_ms) < 0)) {
        return false;
    }
    dx = next_offset(stimulus, VISION_TRACKING_STIMULUS_MAX_ERROR_X_PIXELS);
    dy = next_offset(stimulus, VISION_TRACKING_STIMULUS_MAX_ERROR_Y_PIXELS);
    out->target_valid = true;
    out->target_x = (uint16_t)((int32_t)VISION_STIMULUS_CENTER_X + dx);
    out->target_y = (uint16_t)((int32_t)VISION_STIMULUS_CENTER_Y + dy);
    out->frame_width = VISION_STIMULUS_FRAME_WIDTH;
    out->frame_height = VISION_STIMULUS_FRAME_HEIGHT;
    out->received_at_ms = now_ms;
    out->sequence = ++stimulus->sequence;
    stimulus->next_sample_ms = now_ms + VISION_TRACKING_STIMULUS_INTERVAL_MS;
    return true;
}
