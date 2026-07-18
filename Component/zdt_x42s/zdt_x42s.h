/**
 * @file zdt_x42s.h
 * @brief ZDT X42S Emm 固件 UART 位置模式协议。
 */
#ifndef ZDT_X42S_H
#define ZDT_X42S_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZDT_X42S_MAX_SPEED_RPM          3000U
#define ZDT_X42S_MAX_ACCELERATION_LEVEL 255U
#define ZDT_X42S_DEFAULT_PULSES_PER_REV  3200U

typedef enum {
    ZDT_X42S_DIRECTION_CW = 0U,
    ZDT_X42S_DIRECTION_CCW = 1U
} zdt_x42s_direction_t;

typedef enum {
    ZDT_X42S_RESPONSE_NONE = 0U,
    ZDT_X42S_RESPONSE_ACCEPTED,
    ZDT_X42S_RESPONSE_REACHED,
    ZDT_X42S_RESPONSE_PROTECTION_ERROR,
    ZDT_X42S_RESPONSE_PROTOCOL_ERROR
} zdt_x42s_response_t;

typedef struct {
    zdt_x42s_direction_t direction;
    uint16_t speed_rpm;
    uint8_t acceleration;
    uint32_t pulse_count;
    uint8_t motion_mode;
    uint8_t sync_flag;
} zdt_x42s_move_t;

typedef bool (*zdt_x42s_write_fn)(void *context,
                                  const uint8_t *data,
                                  size_t length);
typedef bool (*zdt_x42s_read_fn)(void *context, uint8_t *byte);

typedef struct {
    zdt_x42s_write_fn write;
    zdt_x42s_read_fn read;
    void *context;
} zdt_x42s_transport_t;

typedef struct {
    zdt_x42s_transport_t transport;
    uint8_t address;
    uint8_t response_bytes[4];
    uint8_t response_count;
    zdt_x42s_response_t last_response;
} zdt_x42s_t;

bool zdt_x42s_init(zdt_x42s_t *motor,
                   const zdt_x42s_transport_t *transport,
                   uint8_t address);
bool zdt_x42s_set_enabled(zdt_x42s_t *motor, bool enabled);
bool zdt_x42s_start_move(zdt_x42s_t *motor, const zdt_x42s_move_t *move);
zdt_x42s_response_t zdt_x42s_consume_response_byte(zdt_x42s_t *motor,
                                                   uint8_t byte);
zdt_x42s_response_t zdt_x42s_poll_response(zdt_x42s_t *motor);

#endif /* ZDT_X42S_H */
