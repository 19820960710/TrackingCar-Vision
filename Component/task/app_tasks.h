/**
 * @file    app_tasks.h
 * @brief   应用层 FreeRTOS 任务与对外控制/状态接口。
 *
 * @details 本文件定义了应用层对外暴露的全部数据结构和控制/状态接口。
 *          外部调用方（如 OLED、调试串口、远程控制）只需包含本头文件，
 *          无需了解 service/control/pid 各层的内部实现。
 *
 *          使用示例：
 *          @code
 *          // 让小车以 60 RPM 速度向前，同时转向到 90°
 *          app_tasks_set_yaw_target(60, 900);
 *          // 等待到位（超时 3 秒）
 *          int ret = app_tasks_wait_yaw_settled(3000);
 *          // 停止
 *          app_tasks_set_wheel_speed_target(0, 0);
 *          @endcode
 */
#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  yaw 角闭环目标结构体。
 * @note   角度单位均为 0.1°（deg10），即 45°=450，-90°=-900。
 */
typedef struct {
    int32_t base_speed_rpm;   /**< 基准速度 (RPM)，正数=前进，负数=后退 */
    int32_t target_yaw_deg10; /**< 期望偏航角 (×10)，会自动归一化到 ±180° */
} app_yaw_target_t;

/**
 * @brief  对外姿态快照。
 * @note   角度单位均为 0.1°（deg10）。
 *         valid=false 表示 MPU 未稳定或通信故障，下游应做安全处理。
 */
typedef struct {
    int32_t pitch_deg10;      /**< 俯仰角 ×10，单位 0.1° */
    int32_t roll_deg10;       /**< 横滚角 ×10，单位 0.1° */
    int32_t yaw_deg10;        /**< 偏航角 ×10，单位 0.1°（已减去零点偏移） */
    bool valid;               /**< true=MPU 已稳定且数据有效 */
} app_attitude_t;

/**
 * @brief  对外两轮速度快照。
 *         不包含 PWM/PID 内部状态，只暴露目标与实测 RPM。
 */
typedef struct {
    int32_t left_rpm;         /**< 左轮实测速度 (RPM) */
    int32_t right_rpm;        /**< 右轮实测速度 (RPM) */
    int32_t left_target_rpm;  /**< 左轮目标速度 (RPM) */
    int32_t right_target_rpm; /**< 右轮目标速度 (RPM) */
    bool stopped;             /**< true=两轮已完全停稳（目标、设定点、PWM 均归零） */
} app_wheel_speed_t;

/**
 * @brief  对外 yaw 闭环状态快照。
 *         不包含 PID 内部状态，对外暴露控制结果。
 */
typedef struct {
    int32_t base_speed_rpm;      /**< 当前基准速度 (RPM) */
    int32_t target_yaw_deg10;    /**< 当前目标偏航角 ×10 */
    int32_t current_yaw_deg10;   /**< 当前实际偏航角 ×10 */
    int32_t error_yaw_deg10;     /**< 控制误差 ×10（目标 - 当前，已归一化） */
    int32_t turn_rpm;            /**< yaw 环输出的差速转向量 (RPM) */
    bool enabled;                /**< true=yaw 闭环已使能 */
    bool settled;                /**< true=已进入到位保持区（误差 < 死区） */
    bool speed_ff_enable;        /**< true=请求速度环启用 yaw 专用低速前馈 */
    bool attitude_valid;         /**< true=本控制周期姿态可用；false 时上层应安全停车 */
} app_yaw_status_t;

/**
 * @brief  应用层聚合状态快照。
 *         一次调用获取全部可用状态，各模块的可用性通过 xxx_available 字段区分。
 */
typedef struct {
    app_attitude_t attitude;      /**< 姿态快照 */
    app_wheel_speed_t wheel_speed; /**< 轮速度快照 */
    app_yaw_status_t yaw;          /**< yaw 闭环状态 */
    bool attitude_available;      /**< true=姿态数据有效 */
    bool wheel_speed_available;   /**< true=轮速数据有效 */
    bool yaw_status_available;    /**< true=yaw 状态有效 */
} app_vehicle_state_t;

/**
 * @brief  设置左右轮目标速度。
 * @param  left_rpm  左轮目标 RPM（正=前进，负=后退，0=停止）
 * @param  right_rpm 右轮目标 RPM
 * @return true=目标已写入队列
 * @note   本接口默认关闭低速前馈（yaw 环通常用 set_yaw_target 走前馈路径）。
 *         用于不带 yaw 环的直行、差速转向、原地旋转等运动。
 */
bool app_tasks_set_wheel_speed_target(int32_t left_rpm, int32_t right_rpm);

/**
 * @brief  设置 yaw 闭环目标。
 * @param  base_speed_rpm   基准前进速度 (RPM)，差速转向时作为中值
 * @param  target_yaw_deg10 目标偏航角 ×10（例：45°=450），自动归一化到 ±180°
 * @return true=目标已写入队列
 * @note   yaw 环内部 50ms 执行一次，目标斜坡 15°/s 平滑过渡。
 *         调用后可用 wait_yaw_settled() 等待到位完成。
 */
bool app_tasks_set_yaw_target(int32_t base_speed_rpm, int32_t target_yaw_deg10);

/**
 * @brief  等待 yaw 到位且两轮停止。
 * @param  timeout_ms  超时 (ms)。0=非阻塞轮询，立即返回。
 * @return 0=已到位或超时；1=未到位（仅 timeout_ms==0 时）
 * @note   超时返回 0 并非成功到位，调用需检查状态区分。
 */
int app_tasks_wait_yaw_settled(uint32_t timeout_ms);

/**
 * @brief  获取最新姿态快照（只读 peek）。
 * @param  out  输出缓冲区（非空）
 * @return true=成功读到姿态数据
 */
bool app_tasks_get_attitude(app_attitude_t *out);

/**
 * @brief  获取两轮速度快照。
 * @param  out  输出缓冲区（非空）
 * @return true=成功获取
 */
bool app_tasks_get_wheel_speed(app_wheel_speed_t *out);

/**
 * @brief  获取 yaw 闭环状态快照。
 * @param  out  输出缓冲区（非空）
 * @return true=成功获取
 */
bool app_tasks_get_yaw_status(app_yaw_status_t *out);

/**
 * @brief  获取车辆聚合状态（姿态+轮速+yaw）。
 * @param  out  输出缓冲区（非空），先清零再填充
 * @return true=至少一个模块有数据
 */
bool app_tasks_get_vehicle_state(app_vehicle_state_t *out);

/**
 * @brief  创建所有 FreeRTOS 任务并启动调度器。
 * @note   成功后不返回。应在 GPIO/时钟/外设硬件初始化完成后调用。
 *         内部按优先级创建 6 个任务：YAW_LOOP(4) > SPD_LOOP(3) >
 *         MPU(2) = YAW_KEY(2) > LED(1) = OLED(1)。
 *         任一 service init 失败则死循环停在此处。
 */
void app_tasks_start(void);

#endif /* APP_TASKS_H */
