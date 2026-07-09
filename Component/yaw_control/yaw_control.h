#ifndef YAW_CONTROL_H
#define YAW_CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include "pid/pid.h"

/**
 * @brief yaw 控制器运行输出。
 * @note  app_tasks.c 只关心任务调度和轮速下发，具体 yaw 算法留在本模块。
 */
typedef struct {
    int32_t turn_rpm;              /* yaw 输出差速修正量 */
    int32_t control_error_deg10;   /* 参与控制的误差，单位 0.1° */
    int32_t derr_deg10;            /* 本周期误差变化，单位 0.1°/周期 */
    bool settled;                  /* true=已进入到位/保持区 */
    bool speed_ff_enable;          /* true=允许速度环低速前馈 */
    bool approaching_close;        /* true=接近目标且误差快速变小 */
} yaw_control_output_t;

typedef struct {
    pid_pos_t pid;
    int32_t control_target_deg10;
    bool control_target_initialized;
    bool settled_latch;
    uint8_t reacquire_count;
} yaw_control_t;

int32_t yaw_normalize_deg10(int32_t angle_deg10);
int32_t yaw_float_deg_to_deg10(float angle_deg);
float yaw_normalize_deg(float angle);
int32_t yaw_abs_i32(int32_t value);

void yaw_control_init(yaw_control_t *control);
void yaw_control_reset(yaw_control_t *control);
void yaw_control_update(yaw_control_t *control,
                        int32_t target_yaw_deg10,
                        int32_t current_yaw_deg10,
                        yaw_control_output_t *output);

#endif /* YAW_CONTROL_H */
