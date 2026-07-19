/**
 * @file zdt_x42s.c
 * @brief ZDT X42S 二进制帧组装与四字节响应解析。
 */
#include "zdt_x42s/zdt_x42s.h"

#include <stddef.h>

#define ZDT_FRAME_SUFFIX           0x6BU
#define ZDT_CMD_ENABLE             0xF3U
#define ZDT_CMD_POSITION           0xFDU
#define ZDT_CMD_STOP               0xFEU
#define ZDT_CMD_READ_POSITION      0x36U
#define ZDT_STOP_KEY               0x98U
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

bool zdt_x42s_stop(zdt_x42s_t *motor, uint8_t sync_flag)
{
    uint8_t frame[5];

    if ((motor == NULL) || (sync_flag > 1U)) {
        return false;
    }
    frame[0] = motor->address;
    frame[1] = ZDT_CMD_STOP;
    frame[2] = ZDT_STOP_KEY;
    frame[3] = sync_flag;
    frame[4] = ZDT_FRAME_SUFFIX;
    return send_frame(motor, frame, sizeof(frame));
}

bool zdt_x42s_request_position(zdt_x42s_t *motor)
{
    uint8_t frame[3];

    if (motor == NULL) {
        return false;
    }
    frame[0] = motor->address;
    frame[1] = ZDT_CMD_READ_POSITION;
    frame[2] = ZDT_FRAME_SUFFIX;
    return send_frame(motor, frame, sizeof(frame));
}

bool zdt_x42s_get_position(const zdt_x42s_t *motor, int32_t *position,
                           uint32_t *sequence)
{
    if ((motor == NULL) || (position == NULL) || (sequence == NULL)) {
        return false;
    }
    *position = motor->realtime_position;
    *sequence = motor->position_sequence;
    return motor->position_sequence != 0U;
}

bool zdt_x42s_get_last_control_response(const zdt_x42s_t *motor,
                                        uint8_t *function,
                                        uint8_t *response_code)
{
    if ((motor == NULL) || (function == NULL) || (response_code == NULL)) {
        return false;
    }
    *function = motor->last_control_function;
    *response_code = motor->last_control_code;
    return motor->last_control_function != 0U;
}

static zdt_x42s_response_t parse_response(zdt_x42s_t *motor)
{
    uint8_t response_code = motor->response_bytes[2];

    if (motor->response_bytes[1] == ZDT_CMD_READ_POSITION) {
        uint32_t magnitude;

        if ((motor->response_bytes[0] != motor->address) ||
            (motor->response_length != 8U) ||
            (motor->response_bytes[7] != ZDT_FRAME_SUFFIX)) {
            return ZDT_X42S_RESPONSE_PROTOCOL_ERROR;
        }
        magnitude = ((uint32_t)motor->response_bytes[3] << 24U) |
                    ((uint32_t)motor->response_bytes[4] << 16U) |
                    ((uint32_t)motor->response_bytes[5] << 8U) |
                    (uint32_t)motor->response_bytes[6];
        motor->realtime_position = motor->response_bytes[2] != 0U ?
            -(int32_t)magnitude : (int32_t)magnitude;
        motor->position_sequence++;
        return ZDT_X42S_RESPONSE_POSITION;
    }

    if ((motor->response_bytes[0] != motor->address) ||
        (motor->response_bytes[3] != ZDT_FRAME_SUFFIX)) {
        return ZDT_X42S_RESPONSE_PROTOCOL_ERROR;
    }
    motor->last_control_function = motor->response_bytes[1];
    motor->last_control_code = response_code;
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
    if (motor->response_count >= sizeof(motor->response_bytes)) {
        motor->response_count = 0U;
        motor->response_length = 0U;
        return ZDT_X42S_RESPONSE_PROTOCOL_ERROR;
    }
    motor->response_bytes[motor->response_count++] = byte;
    if (motor->response_count == 2U) {
        motor->response_length = (byte == ZDT_CMD_READ_POSITION) ? 8U : 4U;
    }
    if ((motor->response_length == 0U) ||
        (motor->response_count < motor->response_length)) {
        return ZDT_X42S_RESPONSE_NONE;
    }
    motor->last_response = parse_response(motor);
    motor->response_count = 0U;
    motor->response_length = 0U;
    return motor->last_response;
}
