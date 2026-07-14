#include "vision_packet.h"

#include "vision_config.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static bool consume_prefix(const char **cursor, const char *prefix)
{
    const char *p = *cursor;

    while (*prefix != '\0') {
        if (*p != *prefix) {
            return false;
        }
        p++;
        prefix++;
    }

    *cursor = p;
    return true;
}

static bool parse_i32(const char **cursor, int32_t *out)
{
    const char *p = *cursor;
    uint32_t magnitude = 0U;
    uint32_t limit = (uint32_t) INT32_MAX;
    bool negative = false;
    bool has_digit = false;

    if (*p == '-') {
        negative = true;
        limit++;
        p++;
    }

    while ((*p >= '0') && (*p <= '9')) {
        uint32_t digit = (uint32_t) (*p - '0');

        has_digit = true;
        if (magnitude > ((limit - digit) / 10U)) {
            return false;
        }
        magnitude = (magnitude * 10U) + digit;
        p++;
    }

    if (!has_digit) {
        return false;
    }

    if (negative) {
        *out = (magnitude == limit) ? INT32_MIN : -(int32_t) magnitude;
    } else {
        *out = (int32_t) magnitude;
    }
    *cursor = p;
    return true;
}

static bool consume_comma_and_i32(const char **cursor, int32_t *out)
{
    if (**cursor != ',') {
        return false;
    }

    (*cursor)++;
    return parse_i32(cursor, out);
}

static bool parse_nonempty_token(const char **cursor, const char **token,
                                 size_t *token_length, bool require_comma)
{
    const char *start;
    const char *p;

    if (require_comma) {
        if (**cursor != ',') {
            return false;
        }
        (*cursor)++;
    }

    start = *cursor;
    p = start;
    while ((*p != '\0') && (*p != ',')) {
        p++;
    }
    if (p == start) {
        return false;
    }

    *token = start;
    *token_length = (size_t) (p - start);
    *cursor = p;
    return true;
}

static bool token_equals(const char *token, size_t token_length,
                         const char *expected)
{
    size_t expected_length = strlen(expected);

    return (token_length == expected_length) &&
        (memcmp(token, expected, token_length) == 0);
}

static bool value_fits_i16(int32_t value)
{
    return (value >= INT16_MIN) && (value <= INT16_MAX);
}

static bool value_fits_u16(int32_t value)
{
    return (value >= 0) && (value <= (int32_t) UINT16_MAX);
}

static bool parse_aim_line(const char *line, vision_packet_t *out)
{
    int32_t field[VISION_AIM_NUMERIC_FIELD_COUNT] = {0};
    const char *cursor = line;
    const char *target_mode;
    const char *laser_color;
    size_t target_mode_length;
    size_t laser_color_length;

    if (!consume_prefix(&cursor, VISION_AIM_PREFIX) ||
        !parse_i32(&cursor, &field[0])) {
        return false;
    }

    for (uint8_t i = 1U; i < VISION_AIM_NUMERIC_FIELD_COUNT; i++) {
        if (!consume_comma_and_i32(&cursor, &field[i])) {
            return false;
        }
    }

    if ((field[0] < 0) || (field[0] > 1) || !value_fits_i16(field[1]) ||
        !value_fits_i16(field[2]) || !value_fits_u16(field[3]) ||
        !value_fits_u16(field[4]) || !value_fits_u16(field[5]) ||
        !value_fits_u16(field[6]) ||
        !parse_nonempty_token(&cursor, &target_mode, &target_mode_length, true) ||
        !parse_nonempty_token(&cursor, &laser_color, &laser_color_length, true) ||
        (*cursor != '\0')) {
        return false;
    }

    out->protocol = VISION_OBSERVATION_PROTOCOL_AIM;
    out->aim_valid = (field[0] == 1);
    out->target_valid = !token_equals(target_mode, target_mode_length, "NO_TARGET") &&
        !token_equals(target_mode, target_mode_length, "LOST");
    out->laser_valid = !token_equals(laser_color, laser_color_length, "NO_LASER") &&
        !token_equals(laser_color, laser_color_length, "LOST");
    out->aim_dx = (int16_t) field[1];
    out->aim_dy = (int16_t) field[2];
    out->target_x = (uint16_t) field[3];
    out->target_y = (uint16_t) field[4];
    out->laser_x = (uint16_t) field[5];
    out->laser_y = (uint16_t) field[6];

    return !out->aim_valid || (out->target_valid && out->laser_valid);
}

static bool parse_tv_line(const char *line, vision_packet_t *out)
{
    int32_t field[VISION_TV_NUMERIC_FIELD_COUNT] = {0};
    const char *cursor = line;
    const char *target_mode;
    size_t target_mode_length;

    if (!consume_prefix(&cursor, VISION_TV_PREFIX) ||
        !parse_i32(&cursor, &field[0])) {
        return false;
    }

    for (uint8_t i = 1U; i < VISION_TV_NUMERIC_FIELD_COUNT; i++) {
        if (!consume_comma_and_i32(&cursor, &field[i])) {
            return false;
        }
    }

    if ((field[0] < 0) || (field[0] > 1) || !value_fits_i16(field[1]) ||
        !value_fits_i16(field[2]) || !value_fits_u16(field[3]) ||
        !value_fits_u16(field[4]) ||
        !parse_nonempty_token(&cursor, &target_mode, &target_mode_length, true) ||
        (*cursor != '\0')) {
        return false;
    }

    out->protocol = VISION_OBSERVATION_PROTOCOL_TV;
    out->aim_valid = false;
    out->target_valid = (field[0] == 1) &&
        !token_equals(target_mode, target_mode_length, "NO_TARGET") &&
        !token_equals(target_mode, target_mode_length, "LOST");
    out->laser_valid = false;
    out->aim_dx = 0;
    out->aim_dy = 0;
    out->target_x = (uint16_t) field[3];
    out->target_y = (uint16_t) field[4];
    out->laser_x = 0U;
    out->laser_y = 0U;
    return true;
}

bool vision_packet_parse_line(const char *line, vision_packet_t *out)
{
    if ((line == NULL) || (out == NULL)) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    if ((line[0] == 'A') && (line[1] == 'I')) {
        return parse_aim_line(line, out);
    }
    if ((line[0] == 'T') && (line[1] == 'V')) {
        return parse_tv_line(line, out);
    }
    return false;
}

bool vision_packet_parse_aim_line(const char *line, vision_packet_t *out)
{
    return vision_packet_parse_line(line, out) &&
        (out->protocol == VISION_OBSERVATION_PROTOCOL_AIM);
}

bool vision_packet_to_observation(const vision_packet_t *packet,
                                  uint16_t frame_width,
                                  uint16_t frame_height,
                                  uint32_t sequence,
                                  uint32_t received_at_ms,
                                  vision_observation_t *out)
{
    if ((packet == NULL) || (out == NULL) || (frame_width == 0U) ||
        (frame_height == 0U) ||
        (packet->protocol == VISION_OBSERVATION_PROTOCOL_UNKNOWN)) {
        return false;
    }

    if (packet->target_valid && ((packet->target_x >= frame_width) ||
                                 (packet->target_y >= frame_height))) {
        return false;
    }

    out->target_valid = packet->target_valid;
    out->target_x = packet->target_x;
    out->target_y = packet->target_y;
    out->frame_width = frame_width;
    out->frame_height = frame_height;
    out->sequence = sequence;
    out->received_at_ms = received_at_ms;
    out->protocol = packet->protocol;
    return true;
}
