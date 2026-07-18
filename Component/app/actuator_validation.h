/**
 * @file actuator_validation.h
 * @brief 左右轮与 yaw/pitch 步进轴组合验证任务。
 */
#ifndef ACTUATOR_VALIDATION_H
#define ACTUATOR_VALIDATION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ACTUATOR_VALIDATION_WAITING = 0,
    ACTUATOR_VALIDATION_STAGE_LEFT_AND_YAW,
    ACTUATOR_VALIDATION_INTER_STAGE_STOP,
    ACTUATOR_VALIDATION_STAGE_ALL,
    ACTUATOR_VALIDATION_STAGE_ALL_HOLD,
    ACTUATOR_VALIDATION_FINISHED,
    ACTUATOR_VALIDATION_ERROR
} actuator_validation_stage_t;

/**
 * @brief 供 CMSIS-DAP 调试器连续读取的执行机构验证快照。
 * @note 仅由 actuator_validation 模块写入；sequence 为偶数时才是一致快照。
 */
typedef struct {
    volatile uint32_t sequence;
    volatile uint32_t stage;
    volatile uint32_t yaw_enabled;
    volatile uint32_t pitch_enabled;
    volatile uint32_t yaw_transmitted_commands;
    volatile uint32_t pitch_transmitted_commands;
    volatile uint32_t yaw_last_tx_ok;
    volatile uint32_t pitch_last_tx_ok;
    volatile uint32_t yaw_last_response;
    volatile uint32_t pitch_last_response;
} actuator_validation_debug_snapshot_t;

extern volatile actuator_validation_debug_snapshot_t
    g_actuator_validation_debug;

actuator_validation_stage_t actuator_validation_get_stage(void);
void actuator_validation_task(void *argument);

#endif /* ACTUATOR_VALIDATION_H */
