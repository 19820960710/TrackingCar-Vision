/**
 * @file    speed_service.h
 * @brief   速度环适配层接口：FreeRTOS 队列 + 编码器读取 + TB6612 输出。
 *
 * @details 本模块是 speed_loop_core（纯算法）与硬件/RTOS 之间的适配层：
 *          - 管理速度目标/状态队列（模块私有句柄，外部不接触）；
 *          - 10ms 节拍读取 encoder_get_data() 喂给算法核心；
 *          - 核心 update 返回 true 时取输出 PWM 写入 tb6612；
 *          - 目标为 0/0 时核心立即停止并返回制动请求，本层调用 tb6612_brake()。
 *
 *          === 数据流 ===
 *          @code
 *          // 设速
 *          speed_service_set_target(60, 60);   // 60 RPM 直行
 *
 *          // 10ms 中断回调中：
 *          speed_service_step_10ms();
 *
 *          // 读状态
 *          speed_service_state_t state;
 *          speed_service_get_state(&state);
 *          @endcode
 *
 *          === 制动说明 ===
 *          brake（短接电机）与 set_speed(0)（停止输出但电机自由滑行）不同。
 *          brake 通过短接电机绕组产生电磁阻尼，使电机更快停止。
 *          速度环在以下情况自动触发 brake：
 *          - set_target(0, 0) 时立即制动
 *          - 速度稳定归零后保持制动状态
 */
#ifndef SPEED_SERVICE_H
#define SPEED_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

/** 速度环控制周期 50ms（与 speed_loop_core 一致）。 */
#define SPEED_SERVICE_PERIOD_MS 50U

/**
 * @brief  速度环对外状态快照。
 *         不包含 PWM/PID 等内部状态，对外透明。
 */
typedef struct {
    int32_t left_rpm;         /**< 左轮实测速度 (RPM) */
    int32_t right_rpm;        /**< 右轮实测速度 (RPM) */
    int32_t left_target_rpm;  /**< 左轮目标速度 (RPM) */
    int32_t right_target_rpm; /**< 右轮目标速度 (RPM) */
    bool stopped;             /**< true=目标、斜坡设定点和 PWM 均已归零 */
} speed_service_state_t;

/**
 * @brief  初始化速度环：创建队列、初始化核心与编码器，发布初始停稳快照。
 * @return true=成功，false=队列创建失败
 * @note   应在 FreeRTOS 调度器启动前、编码器 GPIO 初始化后调用。
 */
bool speed_service_init(void);

/**
 * @brief  设置左右轮目标速度 (RPM)，不启用低速前馈。
 *
 * @param  left_rpm   左轮目标 RPM（正=前进，负=后退，0=停止）
 * @param  right_rpm  右轮目标 RPM
 * @return true=目标已写入队列
 * @note   本接口默认关闭 yaw 环前馈。如需前馈请用 set_target_with_ff。
 *         目标为 0/0 时内部立即停止并制动。
 */
bool speed_service_set_target(int32_t left_rpm, int32_t right_rpm);

/**
 * @brief  设置左右轮目标速度，并指定是否启用 yaw 环专用低速前馈。
 *
 * @param  left_rpm            左轮目标 RPM
 * @param  right_rpm           右轮目标 RPM
 * @param  low_speed_ff_enable true=启用低速前馈（由 yaw 控制层决定）
 * @return true=目标已写入队列
 */
bool speed_service_set_target_with_ff(int32_t left_rpm,
                                      int32_t right_rpm,
                                      bool low_speed_ff_enable);

/**
 * @brief  10ms 节拍入口：采样编码器增量，累计到 50ms 执行 PID 并输出 PWM。
 * @note   应在 TIMER_0 中断触发的任务上下文中调用，不在 ISR 中直接调用。
 *         内部自动处理：取目标→采样→PID→PWM/制动→发布快照。
 */
void speed_service_step_10ms(void);

/**
 * @brief  只读取出最新速度状态快照。
 * @param  out  [out] 状态快照（非空）
 * @return true=成功，false=队列未就绪或为空
 */
bool speed_service_get_state(speed_service_state_t *out);

/**
 * @brief  便捷查询两轮是否已完全停稳。
 * @return true=已停稳
 * @note   用于 yaw 到位等待组合判定（app_tasks_wait_yaw_settled）。
 */
bool speed_service_wheels_stopped_snapshot(void);

#endif /* SPEED_SERVICE_H */
