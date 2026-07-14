#ifndef GIMBAL_MOTOR_H_
#define GIMBAL_MOTOR_H_

#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    UART_Regs *uart;
    uint8_t address;
    bool positive_is_cw;
} gimbal_motor_config_t;

typedef struct {
    gimbal_motor_config_t config;
} gimbal_motor_t;

bool gimbal_motor_init(gimbal_motor_t *motor,
                       const gimbal_motor_config_t *config);
bool gimbal_motor_set_enabled(const gimbal_motor_t *motor, bool enabled);
bool gimbal_motor_stop(const gimbal_motor_t *motor);
bool gimbal_motor_request_speed(const gimbal_motor_t *motor);
bool gimbal_motor_move_relative(const gimbal_motor_t *motor, int32_t pulses,
                                uint16_t speed_rpm, uint8_t acceleration);

#endif /* GIMBAL_MOTOR_H_ */
