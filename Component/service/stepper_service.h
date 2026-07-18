/**
 * @file stepper_service.h
 * @brief ZDT X42S 的 FreeRTOS 命令队列与状态服务。
 */
#ifndef STEPPER_SERVICE_H
#define STEPPER_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "zdt_x42s/stepper_motor.h"

typedef enum {
    STEPPER_AXIS_YAW = 0,
    STEPPER_AXIS_PITCH,
    STEPPER_AXIS_COUNT
} stepper_axis_t;

typedef struct {
    bool enabled;
    bool command_pending;
    bool last_tx_ok;
    uint32_t transmitted_commands;
    stepper_motor_response_t last_response;
} stepper_service_state_t;

bool stepper_service_init(void);
bool stepper_service_set_axis_enabled(stepper_axis_t axis, bool enabled);
bool stepper_service_move_axis(stepper_axis_t axis,
                               const stepper_motor_move_t *move);
bool stepper_service_get_axis_state(stepper_axis_t axis,
                                    stepper_service_state_t *out);

/* 兼容旧单轴接口：默认操作 yaw 轴。 */
bool stepper_service_set_enabled(bool enabled);
bool stepper_service_move(const stepper_motor_move_t *move);
bool stepper_service_get_state(stepper_service_state_t *out);
void stepper_service_task(void *argument);

#endif /* STEPPER_SERVICE_H */
