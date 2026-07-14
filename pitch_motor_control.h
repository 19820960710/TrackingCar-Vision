#ifndef PITCH_MOTOR_CONTROL_H
#define PITCH_MOTOR_CONTROL_H

#include "stepper_motor.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    StepperMotor driver;
    StepperMotorResponse last_response;
    bool enabled;
} pitch_motor_t;

void pitch_motor_init(pitch_motor_t *motor, UART_Regs *uart, uint8_t address);
void pitch_motor_enable(pitch_motor_t *motor);
bool pitch_motor_move_relative(pitch_motor_t *motor, int32_t pulses,
                               uint16_t speed_rpm, uint8_t acceleration);
void pitch_motor_poll(pitch_motor_t *motor);
void pitch_motor_run_self_test(pitch_motor_t *motor);

#endif /* PITCH_MOTOR_CONTROL_H */
