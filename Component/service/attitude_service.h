/**
 * @file    attitude_service.h
 * @brief   姿态 service 层接口：MPU 稳定检测、yaw 零点归一与姿态快照发布。
 *
 * @details 本模块不直接访问 ICM20602 驱动；attitude_task 读取 Mahony 解算后的
 *          pitch/roll/yaw 传入，经稳定检测后通过队列发布。
 *
 *          === 稳定检测机制 ===
 *          - 复位后等待 ~20s（80 个采样帧 @ 1kHz，实际受 I2C 传输限制更长）
 *          - 三轴波动需在阈值内（pitch/roll: 1.5°, yaw: 1.0°）
 *          - 稳定时锁存 yaw 零点偏移，此后 yaw 减去该偏移并归一化
 *          - 实现上电自动归零：小车放置方向即为 0°
 *
 *          === 队列模型 ===
 *          姿态通过长度为 1 的队列（覆盖写）发布，外部用 get() 非破坏性读取。
 *
 *          === 使用流程 ===
 *          @code
 *          attitude_service_init();
 *          attitude_service_reset();            // 启动稳定检测
 *          // 任务循环中每帧：
 *          attitude_service_process_sample(p, r, y);
 *          // 下游读取：
 *          app_attitude_t att;
 *          if (attitude_service_get(&att) && att.valid) { ... }
 *          @endcode
 */
#ifndef ATTITUDE_SERVICE_H
#define ATTITUDE_SERVICE_H

#include "task/app_tasks.h"
#include <stdbool.h>

/**
 * @brief  初始化姿态 service：创建姿态队列并复位稳定检测器。
 * @return true=创建成功，false=队列创建失败（内存不足）
 * @note   应在 FreeRTOS 调度器启动前调用。
 */
bool attitude_service_init(void);

/**
 * @brief  复位稳定检测器（清零窗口计数与上下界），重新开始稳定判定。
 * @note   通常在 MPU 初始化完成后调用，第一次上电时自动调用。
 *         如果运行中 MPU 出现异常需要重新稳定，也可手动调用。
 */
void attitude_service_reset(void);

/**
 * @brief  发布一条 valid=false 的姿态，用于 MPU 初始化失败时通知下游。
 * @note   下游（yaw_loop_service）收到 valid=false 后会执行安全停车。
 */
void attitude_service_publish_invalid(void);

/**
 * @brief  喂入一帧 Mahony 解算后的姿态采样。
 *
 *         - 稳定前：内部累计窗口，不发布姿态（静默等待稳定）
 *         - 稳定后：yaw 减去零点偏移并归一化，发布有效姿态
 *         - 稳定判定失败：重置窗口重新累计
 *
 * @param  pitch_deg  pitch 角度（度）
 * @param  roll_deg   roll 角度（度）
 * @param  yaw_deg    yaw 角度（度，原始值，未减零点偏移）
 */
void attitude_service_process_sample(float pitch_deg,
                                     float roll_deg,
                                     float yaw_deg);

/**
 * @brief  只读取出最新姿态快照。
 * @param  out  [out] 姿态快照（非空）
 * @return true=成功，false=队列未就绪或为空
 * @note   本函数使用 xQueuePeek（非破坏性读取），多次调用返回同一值
 *         直到新采样覆盖写入队列。
 */
bool attitude_service_get(app_attitude_t *out);

#endif /* ATTITUDE_SERVICE_H */
