/**
 * @file motor_speed_profiles.h
 * @brief 直流轮毂电机速度环的可选择配置档案。
 *
 * 一个 profile 同时描述机械参数、编码器口径和控制参数。未知参数必须保留
 * 为 0，并通过对应 verified/commissioned 标志表达，禁止借用其他电机数值。
 */
#ifndef MOTOR_SPEED_PROFILES_H
#define MOTOR_SPEED_PROFILES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MOTOR_PROFILE_SIDE_LEFT = 0,
    MOTOR_PROFILE_SIDE_RIGHT,
    MOTOR_SPEED_PROFILE_SIDE_COUNT
} motor_profile_side_t;

typedef enum {
    MOTOR_SIZE_CLASS_LARGE = 0,
    MOTOR_SIZE_CLASS_SMALL
} motor_size_class_t;

typedef struct {
    float kp;
    float ki;
    float kd;
    float integral_limit;
} motor_speed_pid_profile_t;

typedef struct {
    const char *profile_name;
    const char *motor_model;
    motor_size_class_t size_class;
    bool commissioned;

    float nominal_voltage_v;
    float wheel_diameter_mm;
    float gear_ratio_motor_to_output;
    bool gear_ratio_verified;

    /* 电机轴编码器 CPR 与减速比已知时可用于理论核对；控制优先使用下方
     * 输出轴实测 CPR，避免齿隙和商品标称误差进入速度换算。 */
    float encoder_counts_per_motor_rev;
    float output_counts_per_wheel_rev[MOTOR_SPEED_PROFILE_SIDE_COUNT];
    bool output_counts_verified;
    int8_t drive_direction_sign[MOTOR_SPEED_PROFILE_SIDE_COUNT];

    uint32_t sample_period_ms;
    uint32_t control_period_ms;
    float speed_filter_alpha;
    motor_speed_pid_profile_t pid[MOTOR_SPEED_PROFILE_SIDE_COUNT];
    float direction_change_stop_mm_s;
    float validated_min_speed_mm_s;
    float validated_max_speed_mm_s;
    int32_t pwm_logical_max;
} motor_speed_profile_t;

extern const motor_speed_profile_t g_mg680_large_motor_profile;
extern const motor_speed_profile_t g_small_motor_template_profile;

/* 当前车辆的唯一选择点。小电机完成机械标定和 PID 调试后，只改此处。 */
#define MOTOR_SPEED_ACTIVE_PROFILE (&g_mg680_large_motor_profile)

bool motor_speed_profile_is_usable(const motor_speed_profile_t *profile);
float motor_speed_profile_wheel_circumference_mm(
    const motor_speed_profile_t *profile);
float motor_speed_profile_rpm_to_mm_s(const motor_speed_profile_t *profile,
                                      float rpm);
float motor_speed_profile_mm_s_to_rpm(const motor_speed_profile_t *profile,
                                      float speed_mm_s);

#endif /* MOTOR_SPEED_PROFILES_H */
