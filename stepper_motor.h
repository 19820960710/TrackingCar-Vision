#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include "zdt_x42s.h"

typedef ZdtX42sDirection StepperMotorDirection;
typedef ZdtX42sResponse StepperMotorResponse;

typedef struct {
    StepperMotorDirection direction; /**< ZDT X42S 步进电机方向 */
    uint16_t speed_rpm;              /**< ZDT X42S 步进电机速度（0 ~ 3000 RPM） */
    uint8_t acceleration;            /**< ZDT X42S 步进电机加速度档位（0 ~ 255） */
    uint32_t pulse_count;            /**< ZDT X42S 步进电机位置脉冲数（默认 3200 脉冲/圈） */
    uint8_t motion_mode;             /**< ZDT X42S 步进电机位置模式 */
    uint8_t sync_flag;               /**< ZDT X42S 步进电机同步标志 */
} StepperMotorMove;

typedef struct {
    ZdtX42s protocol;
} StepperMotor;

void StepperMotor_init(StepperMotor *motor, UART_Regs *uart, uint8_t address);
bool StepperMotor_enable(StepperMotor *motor);
bool StepperMotor_move(StepperMotor *motor, const StepperMotorMove *move);
bool StepperMotor_stop(StepperMotor *motor);
void StepperMotor_serviceTx(StepperMotor *motor, uint32_t now_ms);
StepperMotorResponse StepperMotor_poll(StepperMotor *motor);

#endif
