#include "stepper_motor.h"

void StepperMotor_init(StepperMotor *motor, UART_Regs *uart, uint8_t address)
{
    ZdtX42s_init(&motor->protocol, uart, address);
}

void StepperMotor_enable(StepperMotor *motor)
{
    ZdtX42s_setEnabled(&motor->protocol, true);
    delay_cycles(100000U);
}

void StepperMotor_move(StepperMotor *motor, const StepperMotorMove *move)
{
    const ZdtX42sMoveEmm command = {
        .direction = move->direction,
        .speed_rpm = move->speed_rpm,
        .acceleration = move->acceleration,
        .pulse_count = move->pulse_count,
        .motion_mode = move->motion_mode,
        .sync_flag = move->sync_flag
    };

    ZdtX42s_startMoveEmm(&motor->protocol, &command);
}

StepperMotorResponse StepperMotor_poll(StepperMotor *motor)
{
    return ZdtX42s_pollResponse(&motor->protocol);
}
