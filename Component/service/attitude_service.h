/**
 * @file    attitude_service.h
 * @brief   姿态 service 层接口：MPU6050 稳定检测、yaw 零点归一与姿态快照发布。
 *
 * @details 本模块不直接访问 MPU6050 驱动；attitude_task 读取 DMP 后将
 *          pitch/roll/yaw 传入本 service。复位后需等待三轴在稳定窗口内
 *          满足阈值（约 20s）才发布有效姿态。姿态通过长度为 1 的队列发布，
 *          外部用 peek 只读取最新值。
 */
#ifndef ATTITUDE_SERVICE_H
#define ATTITUDE_SERVICE_H

#include "task/app_tasks.h"
#include <stdbool.h>

/** 初始化姿态 service：创建姿态队列并复位稳定检测器。成功返回 true。 */
bool attitude_service_init(void);

/** 复位稳定检测器（清零窗口计数与上下界），重新开始稳定判定。 */
void attitude_service_reset(void);

/** 发布一条 valid=false 的姿态，用于 MPU 初始化失败时通知下游。 */
void attitude_service_publish_invalid(void);

/** 喂入一帧 DMP 采样；稳定前不发布，稳定后归一 yaw 零点并发布有效姿态。 */
void attitude_service_process_sample(float pitch_deg,
                                     float roll_deg,
                                     float yaw_deg);

/** 只读取出最新姿态快照；队列未就绪或空时返回 false。 */
bool attitude_service_get(app_attitude_t *out);

#endif /* ATTITUDE_SERVICE_H */
