#ifndef VISION_PACKET_H_
#define VISION_PACKET_H_

#include "vision_observation.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Internal representation of one parsed MaixCAM text line.
 * @details This is protocol-layer data. Gimbal control must use
 *          vision_observation_t instead of this structure.
 */
typedef struct {
    vision_observation_protocol_t protocol; /**< Source text protocol. */
    bool aim_valid;     /**< AIM validity: both target and laser are valid. */
    bool target_valid;  /**< Target validity for this frame. */
    bool laser_valid;   /**< Laser validity for this frame; false for TV. */
    int16_t aim_dx;     /**< Target-minus-laser horizontal error, in pixels. */
    int16_t aim_dy;     /**< Target-minus-laser vertical error, in pixels. */
    uint16_t target_x;  /**< Target-center x coordinate, in pixels. */
    uint16_t target_y;  /**< Target-center y coordinate, in pixels. */
    uint16_t laser_x;   /**< Laser-center x coordinate, in pixels. */
    uint16_t laser_y;   /**< Laser-center y coordinate, in pixels. */
} vision_packet_t;

/** @brief Parse one complete AIM or TV text line from MaixCAM. */
bool vision_packet_parse_line(const char *line, vision_packet_t *out);

/** @brief Compatibility entry point that accepts AIM lines only. */
bool vision_packet_parse_aim_line(const char *line, vision_packet_t *out);

/** @brief Convert a parsed protocol packet to the controller contract. */
bool vision_packet_to_observation(const vision_packet_t *packet,
                                  uint16_t frame_width,
                                  uint16_t frame_height,
                                  uint32_t sequence,
                                  uint32_t received_at_ms,
                                  vision_observation_t *out);

#ifdef __cplusplus
}
#endif

#endif /* VISION_PACKET_H_ */
