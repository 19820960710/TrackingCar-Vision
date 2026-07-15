#ifndef PITCH_MOTOR_CONTROL_H
#define PITCH_MOTOR_CONTROL_H

#include "stepper_motor.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    StepperMotor driver;
    StepperMotorResponse last_response;
    uint32_t command_queued_count;
    uint32_t command_rejected_count;
    uint32_t response_accepted_count;
    uint32_t response_reached_count;
    uint32_t response_protection_count;
    uint32_t response_protocol_error_count;
    bool enabled;
} pitch_motor_t;

typedef struct {
    uint32_t command_queued_count;
    uint32_t command_rejected_count;
    uint32_t tx_frame_queued_count;
    uint32_t tx_frame_completed_count;
    uint32_t tx_busy_reject_count;
    uint32_t tx_timeout_count;
    uint32_t response_accepted_count;
    uint32_t response_reached_count;
    uint32_t response_protection_count;
    uint32_t response_protocol_error_count;
    StepperMotorResponse last_response;
} pitch_motor_diagnostics_t;

typedef enum {
    PITCH_MOTOR_COMMISSIONING_WAIT_START,
    PITCH_MOTOR_COMMISSIONING_WAIT_POSITIVE,
    PITCH_MOTOR_COMMISSIONING_WAIT_NEGATIVE,
    PITCH_MOTOR_COMMISSIONING_COMPLETE,
} pitch_motor_commissioning_state_t;

typedef struct {
    pitch_motor_t *motor;
    pitch_motor_commissioning_state_t state;
    uint32_t state_started_ms;
} pitch_motor_commissioning_t;

void pitch_motor_init(pitch_motor_t *motor, UART_Regs *uart, uint8_t address);
bool pitch_motor_enable(pitch_motor_t *motor);
bool pitch_motor_move_relative(pitch_motor_t *motor, int32_t pulses,
                               uint16_t speed_rpm, uint8_t acceleration);
bool pitch_motor_stop(pitch_motor_t *motor);
void pitch_motor_service_tx(pitch_motor_t *motor, uint32_t now_ms);
void pitch_motor_poll(pitch_motor_t *motor);
void pitch_motor_get_diagnostics(const pitch_motor_t *motor,
                                 pitch_motor_diagnostics_t *out);
void pitch_motor_commissioning_start(pitch_motor_commissioning_t *test,
                                     pitch_motor_t *motor, uint32_t now_ms);
void pitch_motor_commissioning_update(pitch_motor_commissioning_t *test,
                                      uint32_t now_ms);

#endif /* PITCH_MOTOR_CONTROL_H */
