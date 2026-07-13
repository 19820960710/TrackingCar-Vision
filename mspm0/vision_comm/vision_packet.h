#ifndef VISION_PACKET_H_
#define VISION_PACKET_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool aim_valid;
    bool has_target;
    bool has_laser;
    int16_t aim_dx;
    int16_t aim_dy;
    uint16_t target_x;
    uint16_t target_y;
    uint16_t laser_x;
    uint16_t laser_y;
} vision_packet_t;

bool vision_packet_parse_aim_line(const char *line, vision_packet_t *out);

#ifdef __cplusplus
}
#endif

#endif /* VISION_PACKET_H_ */
