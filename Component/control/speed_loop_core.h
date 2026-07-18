/* ============================================================================
 *   闲鱼定制 小研分享屋
 *   任何非闲鱼小研分享屋出售的均为盗版
 *   正式比赛代码绑定机器绑定芯片，任何二手出售均无效
 *   请认准正版
 * ============================================================================ */

/**
 * @file speed_loop_core.h
 * @brief 双轮速度环纯算法，单位为 mm/s，控制周期为 30 ms。
 *
 * 本模块不依赖 FreeRTOS、编码器或 TB6612。采样周期与控制周期由 profile
 * 注入；当前 MG680 档案为每 10 ms 采样、累计 3 次后执行一次位置式 PI。
 */
#ifndef SPEED_LOOP_CORE_H
#define SPEED_LOOP_CORE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SPEED_LOOP_SIDE_LEFT = 0,
    SPEED_LOOP_SIDE_RIGHT,
    SPEED_LOOP_SIDE_COUNT
} speed_loop_side_t;

typedef struct {
    float kp;
    float ki;
    float kd;
    float integral_limit;
    float counts_per_wheel_rev;
} speed_loop_motor_config_t;

typedef struct {
    speed_loop_motor_config_t motor[SPEED_LOOP_SIDE_COUNT];
    float wheel_circumference_mm;
    float filter_alpha;
    float direction_change_stop_mm_s;
    uint32_t sample_period_ms;
    uint32_t control_period_ms;
    int32_t pwm_logical_max;
} speed_loop_config_t;

typedef struct {
    float target_mm_s;
    float raw_speed_mm_s;
    float measured_speed_mm_s;
    float error_mm_s;
    float last_error_mm_s;
    float integral_mm;
    int32_t encoder_delta_sum;
    int32_t pwm_duty_count;
    int8_t active_direction;
    int8_t requested_direction;
    bool direction_change_pending;
} speed_loop_motor_state_t;

typedef struct {
    speed_loop_config_t config;
    speed_loop_motor_state_t motor[SPEED_LOOP_SIDE_COUNT];
    uint32_t sample_count;
} speed_loop_core_t;

typedef struct {
    float left_speed_mm_s;
    float right_speed_mm_s;
    float left_target_mm_s;
    float right_target_mm_s;
    int32_t left_pwm_duty_count;
    int32_t right_pwm_duty_count;
    bool stopped;
    bool brake;
} speed_loop_core_output_t;

void speed_loop_core_init(speed_loop_core_t *ctx,
                          const speed_loop_config_t *config);
void speed_loop_core_set_target_mm_s(speed_loop_core_t *ctx,
                                     float left_mm_s,
                                     float right_mm_s);
bool speed_loop_core_update_sample(speed_loop_core_t *ctx,
                                   int32_t left_delta,
                                   int32_t right_delta,
                                   speed_loop_core_output_t *out);
void speed_loop_core_get_output(const speed_loop_core_t *ctx,
                                speed_loop_core_output_t *out);
bool speed_loop_core_is_stopped(const speed_loop_core_t *ctx);

#endif /* SPEED_LOOP_CORE_H */
