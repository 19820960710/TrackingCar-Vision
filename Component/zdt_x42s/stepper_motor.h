/**
 * @file stepper_motor.h
 * @brief ZDT X42S 的语义化步进电机包装接口。
 */
#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include "zdt_x42s/zdt_x42s.h"

typedef zdt_x42s_move_t stepper_motor_move_t;
typedef zdt_x42s_response_t stepper_motor_response_t;

typedef struct {
    zdt_x42s_t protocol;
} stepper_motor_t;

bool stepper_motor_init(stepper_motor_t *motor,
                        const zdt_x42s_transport_t *transport,
                        uint8_t address);
bool stepper_motor_set_enabled(stepper_motor_t *motor, bool enabled);
bool stepper_motor_move(stepper_motor_t *motor,
                        const stepper_motor_move_t *move);
bool stepper_motor_stop(stepper_motor_t *motor, uint8_t sync_flag);
bool stepper_motor_request_position(stepper_motor_t *motor);
bool stepper_motor_get_position(const stepper_motor_t *motor,
                                int32_t *position, uint32_t *sequence);
bool stepper_motor_get_last_control_response(const stepper_motor_t *motor,
                                             uint8_t *function,
                                             uint8_t *response_code);
stepper_motor_response_t stepper_motor_poll(stepper_motor_t *motor);
stepper_motor_response_t stepper_motor_consume_response_byte(
    stepper_motor_t *motor, uint8_t byte);

#endif /* STEPPER_MOTOR_H */
