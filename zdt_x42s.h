#ifndef ZDT_X42S_H
#define ZDT_X42S_H

#include <stdbool.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"

/* ZDT X42S Emm 固件位置模式（FD）的协议参数范围。 */
#define ZDT_X42S_EMM_MAX_SPEED_RPM          3000U
#define ZDT_X42S_EMM_MAX_ACCELERATION_LEVEL 255U
#define ZDT_X42S_EMM_PULSES_PER_REVOLUTION  3200U

typedef enum {
    ZDT_X42S_DIRECTION_CW = 0U,
    ZDT_X42S_DIRECTION_CCW = 1U
} ZdtX42sDirection;

typedef enum {
    ZDT_X42S_RESPONSE_NONE = 0U,
    ZDT_X42S_RESPONSE_ACCEPTED,
    ZDT_X42S_RESPONSE_REACHED,
    ZDT_X42S_RESPONSE_PROTECTION_ERROR,
    ZDT_X42S_RESPONSE_PROTOCOL_ERROR
} ZdtX42sResponse;

typedef struct {
    ZdtX42sDirection direction; /**< 步进电机方向：CW/CCW */
    uint16_t speed_rpm;         /**< 步进电机速度：0 ~ ZDT_X42S_EMM_MAX_SPEED_RPM */
    uint8_t acceleration;       /**< 步进电机加速度档位：0 ~ ZDT_X42S_EMM_MAX_ACCELERATION_LEVEL */
    uint32_t pulse_count;       /**< 步进电机脉冲数；默认 3200 脉冲为一圈 */
    uint8_t motion_mode;        /**< 步进电机位置模式：0 相对目标、1 绝对零点、2 相对实时位置 */
    uint8_t sync_flag;          /**< 步进电机同步标志：0 立即执行、1 缓存命令 */
} ZdtX42sMoveEmm;

typedef struct {
    UART_Regs *uart;
    uint8_t address;
    uint8_t tx_frame[16];
    uint8_t tx_length;
    uint8_t response_bytes[4];
    uint8_t response_count;
    ZdtX42sResponse last_response;
} ZdtX42s;

void ZdtX42s_init(ZdtX42s *motor, UART_Regs *uart, uint8_t address);
void ZdtX42s_setEnabled(ZdtX42s *motor, bool enabled);
void ZdtX42s_startMoveEmm(ZdtX42s *motor, const ZdtX42sMoveEmm *move);
ZdtX42sResponse ZdtX42s_pollResponse(ZdtX42s *motor);

#endif
