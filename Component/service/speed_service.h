/**
 * @file    speed_service.h
 * @brief   速度环适配层接口：FreeRTOS 队列 + 编码器读取 + TB6612 输出。
 *
 * @details 本模块是 speed_loop_core 纯算法与硬件/RTOS 之间的适配层：
 *          - 管理速度目标/状态队列（模块私有，外部不接触句柄）；
 *          - 读取 encoder_get_data() 喂给算法核心；
 *          - 将核心输出的 PWM 写入 tb6612；
 *          - 对外暴露目标设置与只读状态快照。
 *          纯算法核心封装在 speed_loop_core 中，便于移植/单测。
 */
#ifndef SPEED_SERVICE_H
#define SPEED_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

/** 速度环控制周期 50ms（与 speed_loop_core 一致）。 */
#define SPEED_SERVICE_PERIOD_MS 50U

/** 速度环对外状态快照；不含 PWM/PID 等内部状态。 */
typedef struct {
    int32_t left_rpm;         /**< 左轮实测速度 (RPM) */
    int32_t right_rpm;        /**< 右轮实测速度 (RPM) */
    int32_t left_target_rpm;  /**< 左轮目标速度 (RPM) */
    int32_t right_target_rpm; /**< 右轮目标速度 (RPM) */
    bool stopped;             /**< true=目标、斜坡和输出均已停稳 */
} speed_service_state_t;

/** 初始化速度环：创建队列、初始化核心与编码器，发布初始快照。成功返回 true。 */
bool speed_service_init(void);

/** 设置左右轮目标速度 (RPM)，不启用低速前馈。 */
bool speed_service_set_target(int32_t left_rpm, int32_t right_rpm);

/** 设置左右轮目标速度，并指定是否启用 yaw 环专用低速前馈（应用层默认不用）。 */
bool speed_service_set_target_with_ff(int32_t left_rpm,
                                      int32_t right_rpm,
                                      bool low_speed_ff_enable);

/** 10ms 节拍入口：采样编码器，核心内部累计到 50ms 执行 PID 并输出 PWM。 */
void speed_service_step_10ms(void);

/** 只读取出最新速度状态快照；失败（队列未就绪/空）返回 false。 */
bool speed_service_get_state(speed_service_state_t *out);

/** 便捷查询：两轮是否已完全停稳。用于 yaw 到位等待判断。 */
bool speed_service_wheels_stopped_snapshot(void);

#endif /* SPEED_SERVICE_H */
