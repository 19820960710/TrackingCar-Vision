/**
 * @file    yaw_control.h
 * @brief   yaw 角环纯算法：目标斜坡、位置式 PID、动态限幅、到位锁存与重捕获滞回。
 *
 * @details 单位约定：角度使用 deg10（0.1°），PID 系数使用 milli（×1000）。
 *          本模块为纯算法，不依赖 FreeRTOS/硬件驱动，由 yaw_loop_service 调用。
 *          策略要点（参数见 .c 顶部可调区）：
 *          - 目标斜坡平滑过渡，避免阶跃冲击；
 *          - 动态限幅：误差越大允许越大的转向速度；
 *          - 到位锁存 + 重捕获确认，过滤 MPU 噪声与机械回弹；
 *          - 接近目标且误差快速变小时清零输出，交给速度环短接制动。
 */
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

/**
 * @brief yaw 控制器运行时状态。
 * @note  结构体成员由内部维护，外部只传指针调用 update。
 */
typedef struct {
    pid_pos_t pid;                       /* 位置式 PID 实例 */
    int32_t control_target_deg10;        /* 斜坡后的内部目标角×10 */
    bool control_target_initialized;     /* 内部目标是否已初始化到当前角 */
    bool settled_latch;                  /* 到位锁存标志 */
    uint8_t reacquire_count;             /* 重捕获确认计数 */
} yaw_control_t;

/* 角度归一化到 [-1800, 1800]（即 ±180°，单位 0.1°）。 */
int32_t yaw_normalize_deg10(int32_t angle_deg10);
/* 浮点角度转 deg10（四舍五入）。 */
int32_t yaw_float_deg_to_deg10(float angle_deg);
/* 浮点角度归一化到 (-180, 180]。 */
float yaw_normalize_deg(float angle);
/* 取绝对值（OLED 显示等复用）。 */
int32_t yaw_abs_i32(int32_t value);

/** 初始化 yaw 控制器：装入默认 PID 参数并复位状态。 */
void yaw_control_init(yaw_control_t *control);
/** 复位 yaw 控制器：清 PID 状态、内部目标与到位锁存。 */
void yaw_control_reset(yaw_control_t *control);
/** 执行一次 yaw 角环计算（50ms 周期），输出差速修正与到位/前馈标志。 */
void yaw_control_update(yaw_control_t *control,
                        int32_t target_yaw_deg10,
                        int32_t current_yaw_deg10,
                        yaw_control_output_t *output);

#endif /* YAW_CONTROL_H */
