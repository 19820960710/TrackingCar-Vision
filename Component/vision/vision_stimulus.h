/**
 * @file vision_stimulus.h
 * @brief Deterministic synthetic target source for gimbal command-path checks.
 */
#ifndef VISION_STIMULUS_H
#define VISION_STIMULUS_H

#include <stdbool.h>
#include <stdint.h>

#include "vision/vision_uart.h"

typedef struct {
    uint32_t next_sample_ms;
    uint32_t sequence;
    uint32_t random_state;
} vision_stimulus_t;

void vision_stimulus_init(vision_stimulus_t *stimulus, uint32_t seed);
bool vision_stimulus_take(vision_stimulus_t *stimulus, uint32_t now_ms,
                          vision_observation_t *out);

#endif /* VISION_STIMULUS_H */
