/**
 * @file    yaw_loop_service.h
 * @brief   yaw 角闭环 service 层接口：目标/状态队列、10ms→50ms 分频与串级控制逻辑。
 *
 * @details 本模块持有 yaw 控制器（yaw_control_t），每 50ms 执行一次 yaw PID，
 *          输出 base_speed/turn_rpm/前馈/到位等状态到快照队列。
 *
 *          注意：本模块不做差速换算（left = base - turn, right = base + turn），
 *          也不直接调用 speed_service。差速换算与速度环下发交由 app_tasks 胶水层完成，
 *          使 yaw 和 speed 两环可独立调试/使能。
 *
 *          === 队列模型 ===
 *          目标队列：yaw_target_msg_t（含 base_speed/target_yaw/enable/reset_pid）
 *          状态队列：app_yaw_status_t（含 turn_rpm/settled/speed_ff_enable/attitude_valid）
 *          两个队列长度均为 1，覆盖写，外部通过 setter/getter 访问。
 *
 *          === 使用流程 ===
 *          @code
 *          yaw_loop_service_init();
 *          // 控制循环中每 10ms 调用：
 *          yaw_loop_service_step_10ms();   // 内部分频 + 50ms 控制
 *          // 设置目标：
 *          yaw_loop_service_set_target(60, 900);  // 60 RPM, 90°
 *          // 读取状态：
 *          app_yaw_status_t status;
 *          yaw_loop_service_get_status(&status);
 *          @endcode
 */
#ifndef YAW_LOOP_SERVICE_H
#define YAW_LOOP_SERVICE_H

#include "task/app_tasks.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  初始化 yaw 环：创建目标/状态队列，初始化控制器并写入初始状态。
 * @return true=成功，false=队列创建失败
 * @note   应在 FreeRTOS 调度器启动前调用。
 *         初始状态为：未使能、目标 0°/0 RPM、所有标志清零。
 */
bool yaw_loop_service_init(void);

/**
 * @brief  10ms 节拍入口。
 *
 *         内部处理：
 *         1. 取队列最新目标（带 reset_pid 标志时复位控制器）
 *         2. 5 分频计数器（每 50ms 执行一次 yaw 控制）
 *         3. 满 50ms 时：读姿态 → 算 PID → 写状态快照
 *
 * @return true=本周期完成了 50ms yaw 控制（base/turn/ff 已写入状态）；
 *         false=非 50ms 节拍，yaw 未计算
 * @note   应在 yaw_loop_task 上下文中调用，不在 ISR 中直接调用。
 */
bool yaw_loop_service_step_10ms(void);

/**
 * @brief  设置 yaw 闭环目标。
 *
 * @param  base_speed_rpm   基准前进速度 (RPM)
 * @param  target_yaw_deg10 目标偏航角 ×10（例：45°=450），自动归一化到 ±180°
 * @return true=目标已写入队列
 * @note   本函数会立即更新状态快照中的目标值，无需等下一个 50ms 控制周期。
 *         PID 会被复位（清积分残值），目标斜坡从当前角度开始平滑过渡。
 */
bool yaw_loop_service_set_target(int32_t base_speed_rpm,
                                 int32_t target_yaw_deg10);

/**
 * @brief  只读取出最新 yaw 状态快照。
 * @param  out  [out] yaw 状态（非空）
 * @return true=成功，false=队列未就绪或为空
 */
bool yaw_loop_service_get_status(app_yaw_status_t *out);

/**
 * @brief  便捷查询 yaw 是否已使能且进入到位保持区。
 * @return true=已到位
 * @note   未使能时返回 false，避免误判。
 *         与 speed_service_wheels_stopped_snapshot() 组合使用做完整到位判定。
 */
bool yaw_loop_service_is_settled(void);

#endif /* YAW_LOOP_SERVICE_H */
