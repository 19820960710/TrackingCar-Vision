#include "pitch_motor_control.h"

#include <limits.h>

#define PITCH_MOTOR_MAX_SPEED_RPM        (3000U)
#define PITCH_MOTOR_REALTIME_RELATIVE    (2U)
#define PITCH_MOTOR_EXECUTE_IMMEDIATELY  (0U)
#define PITCH_MOTOR_TEST_PULSES          (20)
#define PITCH_MOTOR_TEST_SPEED_RPM       (20U)
#define PITCH_MOTOR_TEST_ACCELERATION    (10U)

void pitch_motor_init(pitch_motor_t *motor, UART_Regs *uart, uint8_t address)
{
    StepperMotor_init(&motor->driver, uart, address);
    motor->last_response = ZDT_X42S_RESPONSE_NONE;
    motor->enabled = false;
}

void pitch_motor_enable(pitch_motor_t *motor)
{
    StepperMotor_enable(&motor->driver);
    motor->enabled = true;
}

bool pitch_motor_move_relative(pitch_motor_t *motor, int32_t pulses,
                               uint16_t speed_rpm, uint8_t acceleration)
{
    StepperMotorMove command;
    uint32_t magnitude;

    if ((!motor->enabled) || (pulses == 0) ||
        (speed_rpm > PITCH_MOTOR_MAX_SPEED_RPM)) {
        return false;
    }

    magnitude = (pulses == INT32_MIN) ? ((uint32_t) INT32_MAX + 1U) :
        (uint32_t) ((pulses < 0) ? -pulses : pulses);
    command.direction = (pulses > 0) ? ZDT_X42S_DIRECTION_CW :
        ZDT_X42S_DIRECTION_CCW;
    command.speed_rpm = speed_rpm;
    command.acceleration = acceleration;
    command.pulse_count = magnitude;
    command.motion_mode = PITCH_MOTOR_REALTIME_RELATIVE;
    command.sync_flag = PITCH_MOTOR_EXECUTE_IMMEDIATELY;
    StepperMotor_move(&motor->driver, &command);
    return true;
}

void pitch_motor_poll(pitch_motor_t *motor)
{
    motor->last_response = StepperMotor_poll(&motor->driver);
}

void pitch_motor_run_self_test(pitch_motor_t *motor)
{
    static bool positive = true;

    if (positive) {
        (void) pitch_motor_move_relative(motor, PITCH_MOTOR_TEST_PULSES,
                                         PITCH_MOTOR_TEST_SPEED_RPM,
                                         PITCH_MOTOR_TEST_ACCELERATION);
    } else {
        (void) pitch_motor_move_relative(motor, -PITCH_MOTOR_TEST_PULSES,
                                         PITCH_MOTOR_TEST_SPEED_RPM,
                                         PITCH_MOTOR_TEST_ACCELERATION);
    }
    positive = !positive;
    delay_cycles(CPUCLK_FREQ);
}
