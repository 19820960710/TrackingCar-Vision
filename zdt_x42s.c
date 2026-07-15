#include "zdt_x42s.h"

#define ZDT_X42S_FRAME_SUFFIX             (0x6BU)
#define ZDT_X42S_CMD_ENABLE               (0xF3U)
#define ZDT_X42S_CMD_POSITION_TRAPEZOID   (0xFDU)
#define ZDT_X42S_RESPONSE_ACCEPTED_CODE   (0x02U)
#define ZDT_X42S_RESPONSE_REACHED_CODE    (0x9FU)
#define ZDT_X42S_RESPONSE_ERROR_A          (0xE2U)
#define ZDT_X42S_RESPONSE_ERROR_B          (0xEEU)
#define ZDT_X42S_TX_TIMEOUT_MS              (20U)

static bool ZdtX42s_beginFrame(ZdtX42s *motor, uint8_t length)
{
    if ((motor->uart == NULL) || (motor->tx_length != 0U) ||
        (length == 0U) || (length > sizeof(motor->tx_frame))) {
        return false;
    }

    motor->tx_length = length;
    motor->tx_index = 0U;
    motor->tx_timer_started = false;
    motor->tx_frame_queued_count++;
    return true;
}

void ZdtX42s_init(ZdtX42s *motor, UART_Regs *uart, uint8_t address)
{
    motor->uart = uart;
    motor->address = address;
    motor->tx_length = 0U;
    motor->tx_index = 0U;
    motor->tx_started_ms = 0U;
    motor->tx_frame_queued_count = 0U;
    motor->tx_frame_completed_count = 0U;
    motor->tx_busy_reject_count = 0U;
    motor->tx_timeout_count = 0U;
    motor->tx_timer_started = false;
    motor->response_count = 0U;
    motor->last_response = ZDT_X42S_RESPONSE_NONE;
}

bool ZdtX42s_setEnabled(ZdtX42s *motor, bool enabled)
{
    if (motor == NULL) {
        return false;
    }
    if (motor->tx_length != 0U) {
        motor->tx_busy_reject_count++;
        return false;
    }

    motor->tx_frame[0] = motor->address;
    motor->tx_frame[1] = ZDT_X42S_CMD_ENABLE;
    motor->tx_frame[2] = 0xABU;
    motor->tx_frame[3] = enabled ? 0x01U : 0x00U;
    motor->tx_frame[4] = 0x00U;
    motor->tx_frame[5] = ZDT_X42S_FRAME_SUFFIX;
    return ZdtX42s_beginFrame(motor, 6U);
}

bool ZdtX42s_startMoveEmm(ZdtX42s *motor, const ZdtX42sMoveEmm *move)
{
    if ((motor == NULL) || (move == NULL)) {
        return false;
    }
    if (motor->tx_length != 0U) {
        motor->tx_busy_reject_count++;
        return false;
    }

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
    return ZdtX42s_beginFrame(motor, 13U);
}

void ZdtX42s_serviceTx(ZdtX42s *motor, uint32_t now_ms)
{
    if ((motor == NULL) || (motor->uart == NULL) ||
        (motor->tx_length == 0U)) {
        return;
    }

    if (!motor->tx_timer_started) {
        motor->tx_started_ms = now_ms;
        motor->tx_timer_started = true;
    } else if ((uint32_t) (now_ms - motor->tx_started_ms) >=
               ZDT_X42S_TX_TIMEOUT_MS) {
        motor->tx_length = 0U;
        motor->tx_index = 0U;
        motor->tx_timer_started = false;
        motor->tx_timeout_count++;
        return;
    }

    while ((motor->tx_index < motor->tx_length) &&
           !DL_UART_Main_isTXFIFOFull(motor->uart)) {
        DL_UART_Main_transmitData(motor->uart,
                                 motor->tx_frame[motor->tx_index]);
        motor->tx_index++;
    }

    if (motor->tx_index >= motor->tx_length) {
        motor->tx_length = 0U;
        motor->tx_index = 0U;
        motor->tx_timer_started = false;
        motor->tx_frame_completed_count++;
    }
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
