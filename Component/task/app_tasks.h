/**
 * @file    app_tasks.h
 * @brief   应用层 FreeRTOS 任务与对外控制/状态接口
 */
#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdbool.h>
#include <stdint.h>

/** @brief yaw 角闭环目标结构体，角度单位为 0.1°。 */
typedef struct {
    int32_t base_speed_rpm;   /**< 基准速度 (RPM)，正前进/负后退 */
    int32_t target_yaw_deg10; /**< 期望 yaw 角 ×10，例如 45° = 450 */
} app_yaw_target_t;

/** @brief 对外姿态快照；角度单位均为 0.1°。 */
typedef struct {
    int32_t pitch_deg10;
    int32_t roll_deg10;
    int32_t yaw_deg10;
    bool valid;              /**< true=MPU 已稳定且数据有效 */
} app_attitude_t;

/** @brief 对外两轮速度快照；不包含 PWM/PID 等内部状态。 */
typedef struct {
    int32_t left_rpm;
    int32_t right_rpm;
    int32_t left_target_rpm;
    int32_t right_target_rpm;
    bool stopped;
} app_wheel_speed_t;

/** @brief 对外 yaw 闭环状态快照；不包含 PID 内部状态。 */
typedef struct {
    int32_t base_speed_rpm;
    int32_t target_yaw_deg10;
    int32_t current_yaw_deg10;
    int32_t error_yaw_deg10;
    int32_t turn_rpm;
    bool enabled;
    bool settled;
} app_yaw_status_t;

/** @brief 应用层聚合状态快照。 */
typedef struct {
    app_attitude_t attitude;
    app_wheel_speed_t wheel_speed;
    app_yaw_status_t yaw;
    bool attitude_available;
    bool wheel_speed_available;
    bool yaw_status_available;
} app_vehicle_state_t;

bool app_tasks_set_wheel_speed_target(int32_t left_rpm, int32_t right_rpm);
bool app_tasks_set_yaw_target(int32_t base_speed_rpm, int32_t target_yaw_deg10);
int app_tasks_wait_yaw_settled(uint32_t timeout_ms);

bool app_tasks_get_attitude(app_attitude_t *out);
bool app_tasks_get_wheel_speed(app_wheel_speed_t *out);
bool app_tasks_get_yaw_status(app_yaw_status_t *out);
bool app_tasks_get_vehicle_state(app_vehicle_state_t *out);

/**
 * @brief  创建所有 FreeRTOS 任务并启动调度器
 * @note   成功后不返回；应在硬件初始化完成后调用。
 */
void app_tasks_start(void);

#endif /* APP_TASKS_H */
