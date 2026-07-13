#include "vision_packet.h"

#include <stddef.h>

static bool line_starts_with_aim(const char *line)
{
    return (line != NULL) && (line[0] == 'A') && (line[1] == 'I') &&
        (line[2] == 'M') && (line[3] == ',');
}

static bool parse_i32(const char **cursor, int32_t *out)
{
    const char *p = *cursor;
    int32_t sign = 1;
    int32_t value = 0;
    bool has_digit = false;

    if (*p == '-') {
        sign = -1;
        p++;
    }

    while ((*p >= '0') && (*p <= '9')) {
        has_digit = true;
        value = (value * 10) + (int32_t) (*p - '0');
        p++;
    }

    if (!has_digit) {
        return false;
    }

    *out = value * sign;
    *cursor = p;
    return true;
}

static bool parse_next_i32(const char **cursor, int32_t *out)
{
    if (**cursor == ',') {
        (*cursor)++;
    }

    return parse_i32(cursor, out);
}

static uint16_t clamp_u16(int32_t value)
{
    if (value <= 0) {
        return 0U;
    }
    if (value > 65535) {
        return 65535U;
    }
    return (uint16_t) value;
}

static int16_t clamp_i16(int32_t value)
{
    if (value < -32768) {
        return -32768;
    }
    if (value > 32767) {
        return 32767;
    }
    return (int16_t) value;
}

bool vision_packet_parse_aim_line(const char *line, vision_packet_t *out)
{
    int32_t field[7] = {0};
    const char *cursor;

    if ((out == NULL) || !line_starts_with_aim(line)) {
        return false;
    }

    cursor = line + 4;
    for (uint8_t i = 0U; i < 7U; i++) {
        if (!parse_next_i32(&cursor, &field[i])) {
            return false;
        }
    }

    out->aim_valid = (field[0] != 0);
    out->aim_dx = clamp_i16(field[1]);
    out->aim_dy = clamp_i16(field[2]);
    out->target_x = clamp_u16(field[3]);
    out->target_y = clamp_u16(field[4]);
    out->laser_x = clamp_u16(field[5]);
    out->laser_y = clamp_u16(field[6]);
    out->has_target = (out->target_x != 0U) || (out->target_y != 0U);
    out->has_laser = (out->laser_x != 0U) || (out->laser_y != 0U);
    return true;
}
