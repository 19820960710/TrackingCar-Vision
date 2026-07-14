#include "gimbal_motor.h"

#include <limits.h>
#include <stddef.h>

#define EMM_FRAME_TAIL                 (0x6BU)
#define EMM_CMD_ENABLE                 (0xF3U)
#define EMM_CMD_POSITION               (0xFDU)
#define EMM_CMD_STOP                   (0xFEU)
#define EMM_CMD_READ_SPEED             (0x35U)
#define EMM_ENABLE_SUBCOMMAND          (0xABU)
#define EMM_STOP_SUBCOMMAND            (0x98U)
#define EMM_POSITION_RELATIVE_CURRENT  (2U)
#define EMM_MAX_SPEED_RPM              (5000U)

static bool motor_is_ready(const gimbal_motor_t *motor)
{
    return (motor != NULL) && (motor->config.uart != NULL) &&
        (motor->config.address != 0U);
}

static void send_bytes(UART_Regs *uart, const uint8_t *bytes, uint8_t length)
{
    for (uint8_t i = 0U; i < length; i++) {
        while (DL_UART_Main_isTXFIFOFull(uart)) {
        }
        DL_UART_Main_transmitData(uart, bytes[i]);
    }
}

bool gimbal_motor_init(gimbal_motor_t *motor,
                       const gimbal_motor_config_t *config)
{
    if ((motor == NULL) || (config == NULL) || (config->uart == NULL) ||
        (config->address == 0U)) {
        return false;
    }
    motor->config = *config;
    return true;
}

bool gimbal_motor_set_enabled(const gimbal_motor_t *motor, bool enabled)
{
    uint8_t command[6];
    if (!motor_is_ready(motor)) {
        return false;
    }
    command[0] = motor->config.address;
    command[1] = EMM_CMD_ENABLE;
    command[2] = EMM_ENABLE_SUBCOMMAND;
    command[3] = enabled ? 1U : 0U;
    command[4] = 0U;
    command[5] = EMM_FRAME_TAIL;
    send_bytes(motor->config.uart, command, sizeof(command));
    return true;
}

bool gimbal_motor_stop(const gimbal_motor_t *motor)
{
    uint8_t command[5];
    if (!motor_is_ready(motor)) {
        return false;
    }
    command[0] = motor->config.address;
    command[1] = EMM_CMD_STOP;
    command[2] = EMM_STOP_SUBCOMMAND;
    command[3] = 0U;
    command[4] = EMM_FRAME_TAIL;
    send_bytes(motor->config.uart, command, sizeof(command));
    return true;
}

bool gimbal_motor_request_speed(const gimbal_motor_t *motor)
{
    uint8_t command[3];
    if (!motor_is_ready(motor)) {
        return false;
    }
    command[0] = motor->config.address;
    command[1] = EMM_CMD_READ_SPEED;
    command[2] = EMM_FRAME_TAIL;
    send_bytes(motor->config.uart, command, sizeof(command));
    return true;
}

bool gimbal_motor_move_relative(const gimbal_motor_t *motor, int32_t pulses,
                                uint16_t speed_rpm, uint8_t acceleration)
{
    uint8_t command[13];
    uint32_t magnitude;
    bool clockwise;

    if (!motor_is_ready(motor) || (pulses == 0) ||
        (speed_rpm > EMM_MAX_SPEED_RPM)) {
        return false;
    }
    clockwise = (pulses > 0);
    if (!motor->config.positive_is_cw) {
        clockwise = !clockwise;
    }
    magnitude = (pulses == INT32_MIN) ? ((uint32_t) INT32_MAX + 1U) :
        (uint32_t) ((pulses < 0) ? -pulses : pulses);
    command[0] = motor->config.address;
    command[1] = EMM_CMD_POSITION;
    command[2] = clockwise ? 0U : 1U;
    command[3] = (uint8_t) (speed_rpm >> 8);
    command[4] = (uint8_t) speed_rpm;
    command[5] = acceleration;
    command[6] = (uint8_t) (magnitude >> 24);
    command[7] = (uint8_t) (magnitude >> 16);
    command[8] = (uint8_t) (magnitude >> 8);
    command[9] = (uint8_t) magnitude;
    command[10] = EMM_POSITION_RELATIVE_CURRENT;
    command[11] = 0U;
    command[12] = EMM_FRAME_TAIL;
    send_bytes(motor->config.uart, command, sizeof(command));
    return true;
}
