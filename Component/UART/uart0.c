/**
 * @file    uart0.c
 * @brief   UART0 调试串口模块 (FreeRTOS 任务 + ISR 架构)
 * @note
 *   ── 硬件配置 ──
 *   波特率: 115200 8N1
 *   引脚: SysConfig 自动分配
 *
 *   ── 架构设计 ──
 *   发送: 互斥锁保护 uart0_txLock/uart0_txUnlock
 *         调度器启动前直接发送 (无锁), 调度器启动后加锁发送
 *         write() 重定向 libc printf → UART0
 *
 *   接收: ISR 推入 FreeRTOS 队列 → uart0_Recive_task 消费
 *         RX 中断优先级: configLIBRARY_LOWEST_INTERRUPT_PRIORITY (最低)
 *         确保不会阻塞控制相关中断 (TIMG0, GROUP1)
 *
 *   ── 子任务 ──
 *   uart0_Send_task: 每 500ms 发送计数 + 堆使用率 (心跳 + 诊断)
 *   uart0_Recive_task: 回显接收到的字节 (调试用)
 */

#include <stdarg.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include "UART/uart0.h"
#include "ti_msp_dl_config.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  配置常量
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief 发送缓冲区大小 (snprintf 用) */
#define TX_BUF_SIZE  128

/** @brief 接收队列长度 (字节数) */
#define RX_QUEUE_LEN 64

/* ═══════════════════════════════════════════════════════════════════════════
 *  FreeRTOS 同步对象
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief 接收 FIFO 队列: ISR 写入, Recive_task 读取 */
static QueueHandle_t xRxQueue = NULL;

/** @brief 发送互斥锁: 保证多任务打印不交错乱码 */
static SemaphoreHandle_t xTxMutex = NULL;

/* ═══════════════════════════════════════════════════════════════════════════
 *  发送锁 (调度器感知)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  获取发送锁 (调度器启动前后兼容)
 * @note   调度器未启动时: 跳过加锁 (单线程环境, 无竞争)
 *         调度器已启动时: 带超时 (portMAX_DELAY) 获取互斥锁
 *         互斥锁防止多任务同时调用 uart0_sendStr/write() 导致字符交错
 */
static void uart0_txLock(void)
{
    if (xTxMutex != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        xSemaphoreTake(xTxMutex, portMAX_DELAY);
    }
}

/**
 * @brief  释放发送锁
 */
static void uart0_txUnlock(void)
{
    if (xTxMutex != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        xSemaphoreGive(xTxMutex);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  发送函数
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  阻塞发送字符串到 UART0
 * @param  str  NULL 结尾的 ASCII 字符串
 * @note   线程安全: 内部加锁
 *         阻塞式逐字节发送, 长字符串会长时间占用 CPU
 *         调试用接口, 不宜在实时控制任务中调用
 */
void uart0_sendStr(const char *str)
{
    uart0_txLock();
    while (*str) {
        DL_UART_transmitDataBlocking(UART_0_INST, (uint32_t)*str++);
    }
    uart0_txUnlock();
}

/**
 * @brief  libc write() 重定向: printf → UART0
 * @param  fd    文件描述符 (忽略)
 * @param  buf   数据缓冲区
 * @param  size  字节数
 * @return 实际写入的字节数
 * @note   printf() 系列函数最终调用 write(1, ...) 输出
 *         重定向后所有 printf 输出自动发往 UART0 调试串口
 */
int write(int fd, const char *buf, unsigned int size)
{
    (void)fd;  /* 不使用文件描述符 */

    uart0_txLock();
    for (unsigned int i = 0; i < size; i++) {
        DL_UART_transmitDataBlocking(UART_0_INST, (uint32_t)buf[i]);
    }
    uart0_txUnlock();

    return (int)size;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  初始化
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  UART0 初始化 (创建队列 + 互斥锁 + 使能 RX 中断)
 * @note  应在 SYSCFG_DL_init() 之后、调度器启动之前调用
 *        清空 RX FIFO 残留数据, 防止初始化时收到垃圾字节
 */
void uart0_init(void)
{
    /* ── 创建 FreeRTOS 同步对象 ── */
    xRxQueue = xQueueCreate(RX_QUEUE_LEN, sizeof(uint8_t));  /* 接收字节队列 */
    xTxMutex = xSemaphoreCreateMutex();                       /* 发送互斥锁 */

    /* ── 清空 RX FIFO 残留 ──
     * 系统上电或复位时 UART 引脚可能产生虚假边沿,
     * 清空 RX FIFO 避免初始化后立即收到无效字节 */
    uint8_t dummy;
    while (DL_UART_receiveDataCheck(UART_0_INST, &dummy)) {}

    /* ── 使能 RX 中断 ──
     * 优先级: configLIBRARY_LOWEST_INTERRUPT_PRIORITY (最低)
     * 原因: 调试串口不参与实时控制, 不应抢占 TIMG0/GROUP1 */
    NVIC_SetPriority(UART_0_INST_INT_IRQN, configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    DL_UART_enableInterrupt(UART_0_INST, DL_UART_INTERRUPT_RX);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  ISR: UART0 接收中断
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  UART0 接收中断服务例程
 * @note   每收到一个字节, 推入 FreeRTOS 队列供 uart0_Recive_task 消费
 *         xQueueSendFromISR: FreeRTOS FROM_ISR 版本, 在 ISR 中安全使用
 */
void UART_0_INST_IRQHandler(void)
{
    DL_UART_IIDX iid = DL_UART_getPendingInterrupt(UART_0_INST);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (iid == DL_UART_IIDX_RX) {
        uint8_t byte = DL_UART_receiveData(UART_0_INST);  /* 读取接收字节 */

        /* 推入接收队列 (FROM_ISR 版本, 不阻塞) */
        if (xRxQueue != NULL) {
            xQueueSendFromISR(xRxQueue, &byte, &xHigherPriorityTaskWoken);
        }
    }

    /* 若接收任务优先级高于当前任务, 请求上下文切换 */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  FreeRTOS 子任务
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  UART0 发送任务: 心跳打印 + 堆使用率监控
 * @note   每 500ms 发送一次计数信息,
 *         每 5 秒 (10 次 × 500ms) 额外打印堆使用率
 *
 *         输出示例:
 *           Hello UART0 42 [tick=21000]
 *           [Heap] free:12000/16384 (26%)
 */
void uart0_Send_task(void *arg)
{
    int  n   = 0;                     /* 发送计数 */
    char buf[TX_BUF_SIZE];            /* 格式化缓冲区 */

    while (1) {
        TickType_t now = xTaskGetTickCount();

        /* ── 心跳消息 ── */
        snprintf(buf, sizeof(buf), "Hello UART0 %d [tick=%lu]\r\n",
                 n, (unsigned long)now);
        uart0_sendStr(buf);

        /* ── 每 10 次 (≈ 5 秒) 打印堆使用率 ──
         * configTOTAL_HEAP_SIZE 在 FreeRTOSConfig.h 中定义
         * xPortGetFreeHeapSize() 返回当前可用堆字节数 */
        if (n % 10 == 0) {
            size_t freeHeap = xPortGetFreeHeapSize();
            size_t used     = configTOTAL_HEAP_SIZE - freeHeap;
            int    pct      = (used * 100) / configTOTAL_HEAP_SIZE;

            snprintf(buf, sizeof(buf), "[Heap] free:%u/%u (%u%%)\r\n",
                     freeHeap, configTOTAL_HEAP_SIZE, pct);
            uart0_sendStr(buf);
        }

        n++;
        vTaskDelay(pdMS_TO_TICKS(500));  /* 500ms 发送周期 */
    }
}

/**
 * @brief  UART0 接收任务: 回显接收到的字节
 * @note   从接收队列取字节并原样发送回去 (echo)
 *         可用于串口调试助手验证 UART 通信链路正常
 */
void uart0_Recive_task(void *arg)
{
    uint8_t byte;

    while (1) {
        /* 阻塞等待 (portMAX_DELAY): 无数据时让出 CPU */
        if (xQueueReceive(xRxQueue, &byte, portMAX_DELAY) == pdTRUE) {
            /* 回显 (阻塞发送, 不影响 ISR) */
            DL_UART_transmitDataBlocking(UART_0_INST, byte);
        }
    }
}
