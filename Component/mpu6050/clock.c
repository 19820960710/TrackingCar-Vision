/**
 * @file    clock.c
 * @brief   MSPM0 延时与时间戳工具 (FreeRTOS 感知)
 * @note
 *   ── 设计意图 ──
 *   InvenSense 官方 MPU 驱动库需要平台无关的延时和时间戳函数.
 *   本模块提供 mspm0_delay_ms() 和 mspm0_get_clock_ms(),
 *   根据 FreeRTOS 调度器状态自动选择实现:
 *
 *     调度器已启动 → 使用 vTaskDelay / xTaskGetTickCount
 *     调度器未启动 → 使用 busy-loop 延时 (delay_cycles)
 *
 *   ── 使用场景 ──
 *   1. MPU6050_Init() 中 I2C 通信需要 ms 级延时 (调度器未启动时)
 *   2. DMP 固件加载期间的时间跟踪
 *   3. mspm0_i2c.c 中 I2C 总线解锁时的 GPIO bit-bang 延时
 */

#include "clock.h"

#include "FreeRTOS.h"
#include "task.h"
#include "ti_msp_dl_config.h"  /* 提供 CPUCLK_FREQ / delay_cycles */

/**
 * @brief  毫秒延时 (调度器感知)
 * @param  num_ms  延时的毫秒数
 * @return 0
 *
 * @note   调度器已启动: 调用 vTaskDelay() 让出 CPU (任务进入 Blocked 态)
 *         调度器未启动: busy-loop 空转 (delay_cycles(CPUCLK_FREQ/1000) × num_ms)
 *         传入 0: 直接返回 (避免无意义的上下文切换或空转)
 */
int mspm0_delay_ms(unsigned long num_ms)
{
    if (num_ms == 0U) {
        return 0;
    }

    /* 调度器已启动 → 使用 FreeRTOS 任务延时 (让出 CPU) */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        vTaskDelay(pdMS_TO_TICKS(num_ms));
    } else {
        /* 调度器未启动 → busy-loop 空转
         * CPUCLK_FREQ / 1000 = 每 ms 的时钟周期数
         * @ 80MHz: 80000 cycles/ms */
        while (num_ms--) {
            delay_cycles(CPUCLK_FREQ / 1000U);
        }
    }
    return 0;
}

/**
 * @brief  获取系统运行毫秒数 (调度器感知)
 * @param  count  输出参数, 接收毫秒值
 * @return 0=成功, 1=count 为 NULL
 *
 * @note   调度器已启动: 返回 xTaskGetTickCount() × portTICK_PERIOD_MS
 *         调度器未启动: 返回 0 (尚无时间基准)
 */
int mspm0_get_clock_ms(unsigned long *count)
{
    if (count == NULL) {
        return 1;
    }

    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        /* FreeRTOS 滴答 × 每滴答毫秒数 = 系统运行毫秒数
         * 注意: 可能有 ±1 tick 的量化误差 */
        *count = (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    } else {
        *count = 0U;
    }
    return 0;
}

/**
 * @brief  SysTick 初始化 (空实现)
 * @note   FreeRTOS 使用 SysTick 作为滴答定时器, 已在 vPortSetupTimerInterrupt() 中配置
 *          此函数作为 InvenSense 库的平台适配接口, 无需额外操作
 */
void SysTick_Init(void)
{
    /* FreeRTOS 已初始化 SysTick, 无需额外操作 */
}
