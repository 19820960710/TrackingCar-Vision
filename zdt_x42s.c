#include "zdt_x42s.h"

#define ZDT_X42S_FRAME_SUFFIX             (0x6BU)
#define ZDT_X42S_CMD_ENABLE               (0xF3U)
#define ZDT_X42S_CMD_POSITION_TRAPEZOID   (0xFDU)
#define ZDT_X42S_RESPONSE_ACCEPTED_CODE   (0x02U)
#define ZDT_X42S_RESPONSE_REACHED_CODE    (0x9FU)
#define ZDT_X42S_RESPONSE_ERROR_A          (0xE2U)
#define ZDT_X42S_RESPONSE_ERROR_B          (0xEEU)

static void ZdtX42s_sendByte(const ZdtX42s *motor, uint8_t byte)
{
    while (DL_UART_Main_isTXFIFOFull(motor->uart)) {
    }

    DL_UART_Main_transmitData(motor->uart, byte);
}

static void ZdtX42s_sendFrame(const ZdtX42s *motor, const uint8_t *frame, uint8_t length)
{
    uint8_t index;

    for (index = 0U; index < length; ++index) {
        ZdtX42s_sendByte(motor, frame[index]);
    }
}

void ZdtX42s_init(ZdtX42s *motor, UART_Regs *uart, uint8_t address)
{
    motor->uart = uart;
    motor->address = address;
    motor->tx_length = 0U;
    motor->response_count = 0U;
    motor->last_response = ZDT_X42S_RESPONSE_NONE;
}

void ZdtX42s_setEnabled(ZdtX42s *motor, bool enabled)
{
    motor->tx_frame[0] = motor->address;
    motor->tx_frame[1] = ZDT_X42S_CMD_ENABLE;
    motor->tx_frame[2] = 0xABU;
    motor->tx_frame[3] = enabled ? 0x01U : 0x00U;
    motor->tx_frame[4] = 0x00U;
    motor->tx_frame[5] = ZDT_X42S_FRAME_SUFFIX;
    motor->tx_length = 6U;
    ZdtX42s_sendFrame(motor, motor->tx_frame, motor->tx_length);
}

void ZdtX42s_startMoveEmm(ZdtX42s *motor, const ZdtX42sMoveEmm *move)
{
    motor->tx_frame[0] = motor->address;
    motor->tx_frame[1] = ZDT_X42S_CMD_POSITION_TRAPEZOID;
    motor->tx_frame[2] = (uint8_t) move->direction;
    motor->tx_frame[3] = (uint8_t) (move->speed_rpm >> 8U);
    motor->tx_frame[4] = (uint8_t) move->speed_rpm;
    motor->tx_frame[5] = move->acceleration;
    motor->tx_frame[6] = (uint8_t) (move->pulse_count >> 24U);
    motor->tx_frame[7] = (uint8_t) (move->pulse_count >> 16U);
    motor->tx_frame[8] = (uint8_t) (move->pulse_count >> 8U);
    motor->tx_frame[9] = (uint8_t) move->pulse_count;
    motor->tx_frame[10] = move->motion_mode;
    motor->tx_frame[11] = move->sync_flag;
    motor->tx_frame[12] = ZDT_X42S_FRAME_SUFFIX;
    motor->tx_length = 13U;
    ZdtX42s_sendFrame(motor, motor->tx_frame, motor->tx_length);
}

ZdtX42sResponse ZdtX42s_pollResponse(ZdtX42s *motor)
{
    uint8_t response_code;

    while ((!DL_UART_Main_isRXFIFOEmpty(motor->uart)) &&
           (motor->response_count < sizeof(motor->response_bytes))) {
        motor->response_bytes[motor->response_count] =
            DL_UART_Main_receiveData(motor->uart);
        ++motor->response_count;
    }

    if (motor->response_count < sizeof(motor->response_bytes)) {
        return ZDT_X42S_RESPONSE_NONE;
    }

    response_code = motor->response_bytes[2];
    if ((motor->response_bytes[0] != motor->address) ||
        (motor->response_bytes[3] != ZDT_X42S_FRAME_SUFFIX)) {
        motor->last_response = ZDT_X42S_RESPONSE_PROTOCOL_ERROR;
    } else if (response_code == ZDT_X42S_RESPONSE_ACCEPTED_CODE) {
        motor->last_response = ZDT_X42S_RESPONSE_ACCEPTED;
    } else if (response_code == ZDT_X42S_RESPONSE_REACHED_CODE) {
        motor->last_response = ZDT_X42S_RESPONSE_REACHED;
    } else if ((response_code == ZDT_X42S_RESPONSE_ERROR_A) ||
               (response_code == ZDT_X42S_RESPONSE_ERROR_B)) {
        motor->last_response = ZDT_X42S_RESPONSE_PROTECTION_ERROR;
    } else {
        motor->last_response = ZDT_X42S_RESPONSE_PROTOCOL_ERROR;
    }

    motor->response_count = 0U;
    return motor->last_response;
}
