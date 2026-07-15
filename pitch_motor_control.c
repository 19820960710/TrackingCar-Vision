#include "pitch_motor_control.h"

#include <limits.h>

#define PITCH_MOTOR_MAX_SPEED_RPM        (3000U)
#define PITCH_MOTOR_REALTIME_RELATIVE    (2U)
#define PITCH_MOTOR_EXECUTE_IMMEDIATELY  (0U)
#define PITCH_MOTOR_TEST_ANGLE_DEGREES   (30U)
#define PITCH_MOTOR_TEST_PULSES                                         \
    ((int32_t) ((ZDT_X42S_EMM_PULSES_PER_REVOLUTION *                  \
                 PITCH_MOTOR_TEST_ANGLE_DEGREES + 180U) / 360U))
#define PITCH_MOTOR_TEST_SPEED_RPM       (20U)
#define PITCH_MOTOR_TEST_ACCELERATION    (10U)
#define PITCH_MOTOR_TEST_WAIT_MS         (2500U)

void pitch_motor_init(pitch_motor_t *motor, UART_Regs *uart, uint8_t address)
{
    StepperMotor_init(&motor->driver, uart, address);
    motor->last_response = ZDT_X42S_RESPONSE_NONE;
    motor->command_queued_count = 0U;
    motor->command_rejected_count = 0U;
    motor->response_accepted_count = 0U;
    motor->response_reached_count = 0U;
    motor->response_protection_count = 0U;
    motor->response_protocol_error_count = 0U;
    motor->enabled = false;
}

bool pitch_motor_enable(pitch_motor_t *motor)
{
    motor->enabled = StepperMotor_enable(&motor->driver);
    return motor->enabled;
}

bool pitch_motor_move_relative(pitch_motor_t *motor, int32_t pulses,
                               uint16_t speed_rpm, uint8_t acceleration)
{
    StepperMotorMove command;
    uint32_t magnitude;

    if ((!motor->enabled) || (pulses == 0) ||
        (speed_rpm > PITCH_MOTOR_MAX_SPEED_RPM)) {
        motor->command_rejected_count++;
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
    if (!StepperMotor_move(&motor->driver, &command)) {
        motor->command_rejected_count++;
        return false;
    }

    motor->command_queued_count++;
    return true;
}

bool pitch_motor_stop(pitch_motor_t *motor)
{
    if ((!motor->enabled) || !StepperMotor_stop(&motor->driver)) {
        motor->command_rejected_count++;
        return false;
    }

    motor->command_queued_count++;
    return true;
}

void pitch_motor_service_tx(pitch_motor_t *motor, uint32_t now_ms)
{
    StepperMotor_serviceTx(&motor->driver, now_ms);
}

void pitch_motor_poll(pitch_motor_t *motor)
{
    StepperMotorResponse response = StepperMotor_poll(&motor->driver);

    if (response == ZDT_X42S_RESPONSE_NONE) {
        return;
    }

    motor->last_response = response;
    switch (response) {
    case ZDT_X42S_RESPONSE_ACCEPTED:
        motor->response_accepted_count++;
        break;
    case ZDT_X42S_RESPONSE_REACHED:
        motor->response_reached_count++;
        break;
    case ZDT_X42S_RESPONSE_PROTECTION_ERROR:
        motor->response_protection_count++;
        break;
    case ZDT_X42S_RESPONSE_PROTOCOL_ERROR:
        motor->response_protocol_error_count++;
        break;
    case ZDT_X42S_RESPONSE_NONE:
    default:
        break;
    }
}

void pitch_motor_get_diagnostics(const pitch_motor_t *motor,
                                 pitch_motor_diagnostics_t *out)
{
    const ZdtX42s *protocol;

    if ((motor == NULL) || (out == NULL)) {
        return;
    }

    protocol = &motor->driver.protocol;
    out->command_queued_count = motor->command_queued_count;
    out->command_rejected_count = motor->command_rejected_count;
    out->tx_frame_queued_count = protocol->tx_frame_queued_count;
    out->tx_frame_completed_count = protocol->tx_frame_completed_count;
    out->tx_busy_reject_count = protocol->tx_busy_reject_count;
    out->tx_timeout_count = protocol->tx_timeout_count;
    out->response_accepted_count = motor->response_accepted_count;
    out->response_reached_count = motor->response_reached_count;
    out->response_protection_count = motor->response_protection_count;
    out->response_protocol_error_count =
        motor->response_protocol_error_count;
    out->last_response = motor->last_response;
}

void pitch_motor_commissioning_start(pitch_motor_commissioning_t *test,
                                     pitch_motor_t *motor, uint32_t now_ms)
{
    test->motor = motor;
    test->state = PITCH_MOTOR_COMMISSIONING_WAIT_START;
    test->state_started_ms = now_ms;
}

void pitch_motor_commissioning_update(pitch_motor_commissioning_t *test,
                                      uint32_t now_ms)
{
    if ((test->motor == NULL) ||
        (test->state == PITCH_MOTOR_COMMISSIONING_COMPLETE) ||
        ((uint32_t)(now_ms - test->state_started_ms) <
         PITCH_MOTOR_TEST_WAIT_MS)) {
        return;
    }

    switch (test->state) {
    case PITCH_MOTOR_COMMISSIONING_WAIT_START:
        if (pitch_motor_move_relative(test->motor, PITCH_MOTOR_TEST_PULSES,
                                      PITCH_MOTOR_TEST_SPEED_RPM,
                                      PITCH_MOTOR_TEST_ACCELERATION)) {
            test->state = PITCH_MOTOR_COMMISSIONING_WAIT_POSITIVE;
            test->state_started_ms = now_ms;
        }
        break;

    case PITCH_MOTOR_COMMISSIONING_WAIT_POSITIVE:
        if (pitch_motor_move_relative(test->motor, -PITCH_MOTOR_TEST_PULSES,
                                      PITCH_MOTOR_TEST_SPEED_RPM,
                                      PITCH_MOTOR_TEST_ACCELERATION)) {
            test->state = PITCH_MOTOR_COMMISSIONING_WAIT_NEGATIVE;
            test->state_started_ms = now_ms;
        }
        break;

    case PITCH_MOTOR_COMMISSIONING_WAIT_NEGATIVE:
        test->state = PITCH_MOTOR_COMMISSIONING_COMPLETE;
        break;

    case PITCH_MOTOR_COMMISSIONING_COMPLETE:
    default:
        break;
    }
}
