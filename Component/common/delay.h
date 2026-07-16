/**
 * @file    delay.h
 * @brief   MSPM0 延时与时间戳接口 (FreeRTOS 感知, 通用)
 *
 * @details 从原 Component/mpu6050/clock 解耦而来, 函数名去除 mspm0 前缀,
 *          作为通用延时/时间戳工具。供 I2C 驱动、传感器初始化、姿态解算等复用。
 */
#ifndef _DELAY_H_
#define _DELAY_H_

#include <stdint.h>

/**
 * @brief  毫秒延时 (调度器感知)
 * @param  num_ms  毫秒数
 * @return 0
 * @note   调度器已启动 → vTaskDelay 让出 CPU; 未启动 → busy-loop 空转。
 */
int delay_ms(uint32_t num_ms);

/**
 * @brief  获取系统运行毫秒数
 * @param  count  输出参数
 * @return 0=成功, 1=count 为 NULL
 * @note   调度器已启动 → xTaskGetTickCount × portTICK_PERIOD_MS;
 *         未启动 → 0。精度为 1 tick (±1 ms)。
 */
int get_time_ms(uint32_t *count);

#endif /* _DELAY_H_ */
