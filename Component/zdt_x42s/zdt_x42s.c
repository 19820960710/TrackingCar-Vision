/**
 * @file zdt_x42s.c
 * @brief ZDT X42S 二进制帧组装与四字节响应解析。
 */
#include "zdt_x42s/zdt_x42s.h"

#include <stddef.h>

#define ZDT_FRAME_SUFFIX           0x6BU
#define ZDT_CMD_ENABLE             0xF3U
#define ZDT_CMD_POSITION           0xFDU
#define ZDT_RESPONSE_ACCEPTED_CODE 0x02U
#define ZDT_RESPONSE_REACHED_CODE  0x9FU
#define ZDT_RESPONSE_ERROR_A       0xE2U
#define ZDT_RESPONSE_ERROR_B       0xEEU

static bool send_frame(zdt_x42s_t *motor,
                       const uint8_t *frame,
                       size_t length)
{
    if ((motor == NULL) || (motor->transport.write == NULL)) {
        return false;
    }
    return motor->transport.write(motor->transport.context, frame, length);
}

bool zdt_x42s_init(zdt_x42s_t *motor,
                   const zdt_x42s_transport_t *transport,
                   uint8_t address)
{
    zdt_x42s_t reset = {0};

    if ((motor == NULL) || (transport == NULL) ||
        (transport->write == NULL) || (transport->read == NULL)) {
        return false;
    }
    *motor = reset;
    motor->transport = *transport;
    motor->address = address;
    return true;
}

bool zdt_x42s_set_enabled(zdt_x42s_t *motor, bool enabled)
{
    uint8_t frame[6];

    if (motor == NULL) {
        return false;
    }
    frame[0] = motor->address;
    frame[1] = ZDT_CMD_ENABLE;
    frame[2] = 0xABU;
    frame[3] = enabled ? 0x01U : 0x00U;
    frame[4] = 0x00U;
    frame[5] = ZDT_FRAME_SUFFIX;
    return send_frame(motor, frame, sizeof(frame));
}

bool zdt_x42s_start_move(zdt_x42s_t *motor, const zdt_x42s_move_t *move)
{
    uint8_t frame[13];

    if ((motor == NULL) || (move == NULL) ||
        (move->speed_rpm > ZDT_X42S_MAX_SPEED_RPM) ||
        (move->motion_mode > 2U) || (move->sync_flag > 1U)) {
        return false;
    }
    frame[0] = motor->address;
    frame[1] = ZDT_CMD_POSITION;
    frame[2] = (uint8_t)move->direction;
    frame[3] = (uint8_t)(move->speed_rpm >> 8U);
    frame[4] = (uint8_t)move->speed_rpm;
    frame[5] = move->acceleration;
    frame[6] = (uint8_t)(move->pulse_count >> 24U);
    frame[7] = (uint8_t)(move->pulse_count >> 16U);
    frame[8] = (uint8_t)(move->pulse_count >> 8U);
    frame[9] = (uint8_t)move->pulse_count;
    frame[10] = move->motion_mode;
    frame[11] = move->sync_flag;
    frame[12] = ZDT_FRAME_SUFFIX;
    return send_frame(motor, frame, sizeof(frame));
}

static zdt_x42s_response_t parse_response(zdt_x42s_t *motor)
{
    uint8_t response_code = motor->response_bytes[2];

    if ((motor->response_bytes[0] != motor->address) ||
        (motor->response_bytes[3] != ZDT_FRAME_SUFFIX)) {
        return ZDT_X42S_RESPONSE_PROTOCOL_ERROR;
    }
    if (response_code == ZDT_RESPONSE_ACCEPTED_CODE) {
        return ZDT_X42S_RESPONSE_ACCEPTED;
    }
    if (response_code == ZDT_RESPONSE_REACHED_CODE) {
        return ZDT_X42S_RESPONSE_REACHED;
    }
    if ((response_code == ZDT_RESPONSE_ERROR_A) ||
        (response_code == ZDT_RESPONSE_ERROR_B)) {
        return ZDT_X42S_RESPONSE_PROTECTION_ERROR;
    }
    return ZDT_X42S_RESPONSE_PROTOCOL_ERROR;
}

zdt_x42s_response_t zdt_x42s_poll_response(zdt_x42s_t *motor)
{
    uint8_t byte;
    zdt_x42s_response_t response;

    if ((motor == NULL) || (motor->transport.read == NULL)) {
        return ZDT_X42S_RESPONSE_PROTOCOL_ERROR;
    }
    while (motor->transport.read(motor->transport.context, &byte)) {
        response = zdt_x42s_consume_response_byte(motor, byte);
        if (response != ZDT_X42S_RESPONSE_NONE) {
            return response;
        }
    }
    return ZDT_X42S_RESPONSE_NONE;
}

zdt_x42s_response_t zdt_x42s_consume_response_byte(zdt_x42s_t *motor,
                                                   uint8_t byte)
{
    if (motor == NULL) {
        return ZDT_X42S_RESPONSE_PROTOCOL_ERROR;
    }
    if ((motor->response_count == 0U) && (byte != motor->address)) {
        return ZDT_X42S_RESPONSE_NONE;
    }
    motor->response_bytes[motor->response_count++] = byte;
    if (motor->response_count < sizeof(motor->response_bytes)) {
        return ZDT_X42S_RESPONSE_NONE;
    }
    motor->last_response = parse_response(motor);
    motor->response_count = 0U;
    return motor->last_response;
}
