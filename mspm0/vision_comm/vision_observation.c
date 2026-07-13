#include "vision_observation.h"

#include <stddef.h>

bool vision_observation_is_fresh(const vision_observation_t *observation,
                                 uint32_t now_ms, uint32_t timeout_ms)
{
    if ((observation == NULL) ||
        (observation->protocol == VISION_OBSERVATION_PROTOCOL_UNKNOWN)) {
        return false;
    }

    return (uint32_t) (now_ms - observation->received_at_ms) <= timeout_ms;
}
