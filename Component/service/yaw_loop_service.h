/**
 * @file    yaw_loop_service.h
 * @brief   yaw 角闭环 service 层接口：目标/状态队列与串级 step 逻辑。
 *
 * @details 本模块持有 yaw 控制器（yaw_control），每 50ms 执行一次 yaw PID，
 *          输出 turn_rpm 经差速换算后下发到 speed_service。目标/状态队列
 *          模块私有，外部通过 setter/getter 访问，不接触队列句柄。
 */
#ifndef YAW_LOOP_SERVICE_H
#define YAW_LOOP_SERVICE_H

#include "task/app_tasks.h"
#include <stdbool.h>
#include <stdint.h>

/** 初始化 yaw 环：创建目标/状态队列，初始化控制器并写入初始状态。成功返回 true。 */
bool yaw_loop_service_init(void);

/** 10ms 节拍入口：取最新目标 + 5 分频，每 50ms 执行一次 yaw 控制。
 *  返回 true 表示本周期完成了 50ms yaw 控制（base/turn/ff 已写入状态）。 */
bool yaw_loop_service_step_10ms(void);

/** 设置 yaw 闭环目标（基准速度 + 目标角×10），立即更新状态快照。 */
bool yaw_loop_service_set_target(int32_t base_speed_rpm,
                                 int32_t target_yaw_deg10);

/** 只读取出最新 yaw 状态快照；失败返回 false。 */
bool yaw_loop_service_get_status(app_yaw_status_t *out);

/** 便捷查询：yaw 是否已使能且进入到位/保持区。 */
bool yaw_loop_service_is_settled(void);

#endif /* YAW_LOOP_SERVICE_H */
