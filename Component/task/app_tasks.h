/**
 * @file    app_tasks.h
 * @brief   应用层 FreeRTOS 任务与中断胶水层接口
 *
 * ── 架构约定 ──
 * - main.c: 硬件初始化 + 调用 app_tasks_start()
 * - app_tasks.c: 全部 FreeRTOS 对象 (任务/队列/ISR/钩子)
 * - 其他模块可通过 app_tasks_set_wheel_speed_target() 下发速度目标
 *
 * ── 扩展思路 ──
 * 后续平衡/巡线任务可调用 app_tasks_set_wheel_speed_target() 直接设定
 * 左右轮差速目标, 无需关心底层 PID/编码器/电机驱动细节.
 */
#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 轮速目标结构体 (通过 target_speed_queue 传递)
 *
 * 正数 = 前进, 负数 = 后退, 单位: RPM
 * 可设置差速: left_rpm ≠ right_rpm 实现转向
 */
typedef struct {
    int32_t left_rpm;   /**< 左轮目标速度 (RPM) */
    int32_t right_rpm;  /**< 右轮目标速度 (RPM) */
} app_wheel_speed_target_t;

/**
 * @brief  设置左右轮目标速度 (外部接口)
 * @param  left_rpm   左轮目标速度 (RPM), 正前进/负后退
 * @param  right_rpm  右轮目标速度 (RPM), 正前进/负后退
 * @return true=写入成功, false=队列未就绪 (越早调用)
 *
 * @note   通过 xQueueOverwrite 写入, 保证不阻塞调用者
 *         可为后续陀螺仪修正/红外巡线任务提供统一的速度控制接口
 *
 * @code
 *   // 直行: 两轮等速
 *   app_tasks_set_wheel_speed_target(200, 200);
 *
 *   // 原地左转: 差速驱动
 *   app_tasks_set_wheel_speed_target(-100, 100);
 *
 *   // 停止
 *   app_tasks_set_wheel_speed_target(0, 0);
 * @endcode
 */
bool app_tasks_set_wheel_speed_target(int32_t left_rpm, int32_t right_rpm);

/**
 * @brief  创建所有 FreeRTOS 任务并启动调度器
 * @note   执行顺序:
 *         1) 创建队列 (attitude/status/target_speed)
 *         2) 创建 5 个任务 (LED/MPU/SPD_LOOP/GEAR/OLED)
 *         3) 调用 vTaskStartScheduler()
 *
 *         成功后不返回; 失败则死循环
 *         应在硬件初始化完成后调用
 */
void app_tasks_start(void);

#endif /* APP_TASKS_H */
