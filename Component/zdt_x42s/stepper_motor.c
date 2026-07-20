/**
 * @file stepper_motor.c
 * @brief ZDT X42S 协议的薄包装实现。
 */
#include "zdt_x42s/stepper_motor.h"

bool stepper_motor_init(stepper_motor_t *motor,
                        const zdt_x42s_transport_t *transport,
                        uint8_t address)
{
    return (motor != NULL) &&
           zdt_x42s_init(&motor->protocol, transport, address);
}

bool stepper_motor_set_enabled(stepper_motor_t *motor, bool enabled)
{
    return (motor != NULL) &&
           zdt_x42s_set_enabled(&motor->protocol, enabled);
}

bool stepper_motor_clear_stall_protection(stepper_motor_t *motor)
{
    return (motor != NULL) &&
           zdt_x42s_clear_stall_protection(&motor->protocol);
}

bool stepper_motor_move(stepper_motor_t *motor,
                        const stepper_motor_move_t *move)
{
    return (motor != NULL) && zdt_x42s_start_move(&motor->protocol, move);
}

bool stepper_motor_stop(stepper_motor_t *motor, uint8_t sync_flag)
{
    return (motor != NULL) && zdt_x42s_stop(&motor->protocol, sync_flag);
}

bool stepper_motor_request_position(stepper_motor_t *motor)
{
    return (motor != NULL) && zdt_x42s_request_position(&motor->protocol);
}

bool stepper_motor_get_position(const stepper_motor_t *motor,
                                int32_t *position, uint32_t *sequence)
{
    return (motor != NULL) &&
           zdt_x42s_get_position(&motor->protocol, position, sequence);
}

bool stepper_motor_get_last_control_response(const stepper_motor_t *motor,
                                             uint8_t *function,
                                             uint8_t *response_code)
{
    return (motor != NULL) && zdt_x42s_get_last_control_response(
        &motor->protocol, function, response_code);
}

stepper_motor_response_t stepper_motor_poll(stepper_motor_t *motor)
{
    return (motor == NULL) ? ZDT_X42S_RESPONSE_PROTOCOL_ERROR :
                             zdt_x42s_poll_response(&motor->protocol);
}

stepper_motor_response_t stepper_motor_consume_response_byte(
    stepper_motor_t *motor, uint8_t byte)
{
    return (motor == NULL) ? ZDT_X42S_RESPONSE_PROTOCOL_ERROR :
        zdt_x42s_consume_response_byte(&motor->protocol, byte);
}
