/**
 * @file    clock.h
 * @brief   MSPM0 延时与时间戳接口 (FreeRTOS 感知)
 */
#ifndef _CLOCK_H_
#define _CLOCK_H_

/**
 * @brief  毫秒延时 (调度器感知)
 * @param  num_ms  毫秒数
 * @return 0
 */
int mspm0_delay_ms(unsigned long num_ms);

/**
 * @brief  获取系统运行毫秒数
 * @param  count  输出参数
 * @return 0=成功, 1=count 为 NULL
 */
int mspm0_get_clock_ms(unsigned long *count);

/**
 * @brief  SysTick 初始化 (空实现, FreeRTOS 已接管)
 */
void SysTick_Init(void);

#endif /* _CLOCK_H_ */
