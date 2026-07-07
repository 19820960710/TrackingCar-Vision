/**
 * @file    uart0.h
 * @brief   UART0 调试串口模块接口定义
 *
 * ── 功能说明 ──
 * - uart0_init(): 初始化串口 + 创建 FreeRTOS 队列/互斥锁
 * - uart0_sendStr(): 线程安全的阻塞式字符串发送
 * - write(): libc printf 重定向, 所有 printf 输出自动发往 UART0
 * - uart0_Send_task: 心跳打印 + 堆使用率监控 (每 500ms)
 * - uart0_Recive_task: 回显接收字节 (echo 调试)
 *
 * ── 使用说明 ──
 * 本模块的两个 FreeRTOS 任务目前未在 app_tasks_start() 中创建,
 * 如需使用 UART 调试功能, 请手动添加:
 * @code
 *   xTaskCreate(uart0_Send_task,   "UART_TX", 256, NULL, 1, NULL);
 *   xTaskCreate(uart0_Recive_task, "UART_RX", 128, NULL, 1, NULL);
 * @endcode
 */
#ifndef UART0_H
#define UART0_H

#include <stdint.h>

/**
 * @brief  初始化 UART0 调试串口
 * @note   创建接收队列和发送互斥锁, 清空 RX FIFO, 使能 RX 中断
 *         应在 SYSCFG_DL_init() 之后、调度器启动之前调用
 */
void uart0_init(void);

/**
 * @brief  阻塞发送字符串到 UART0 (线程安全)
 * @param  str  NULL 结尾的 ASCII 字符串
 */
void uart0_sendStr(const char *str);

/**
 * @brief  UART0 发送任务入口 (心跳 + 堆监控)
 * @param  arg  任务参数 (未使用)
 * @note   需通过 xTaskCreate() 创建
 */
void uart0_Send_task(void *arg);

/**
 * @brief  UART0 接收任务入口 (echo 回显)
 * @param  arg  任务参数 (未使用)
 * @note   需通过 xTaskCreate() 创建
 */
void uart0_Recive_task(void *arg);

#endif /* UART0_H */
