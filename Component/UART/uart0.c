#include <stdarg.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include "UART/uart0.h"
#include "ti_msp_dl_config.h"

#define TX_BUF_SIZE  128
#define RX_QUEUE_LEN 64

static QueueHandle_t xRxQueue = NULL;
static SemaphoreHandle_t xTxMutex = NULL;

/* 互斥锁：仅在调度器运行时使用，初始化阶段直接发送 */
static void uart0_txLock(void)
{
    if (xTxMutex != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        xSemaphoreTake(xTxMutex, portMAX_DELAY);
    }
}
static void uart0_txUnlock(void)
{
    if (xTxMutex != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        xSemaphoreGive(xTxMutex);
    }
}

void uart0_sendStr(const char *str)
{
    uart0_txLock();
    while (*str) {
        DL_UART_transmitDataBlocking(UART_0_INST, (uint32_t)*str++);
    }
    uart0_txUnlock();
}

/* libc printf 重定向 */
int write(int fd, const char *buf, unsigned int size)
{
    (void)fd;
    uart0_txLock();
    for (unsigned int i = 0; i < size; i++) {
        DL_UART_transmitDataBlocking(UART_0_INST, (uint32_t)buf[i]);
    }
    uart0_txUnlock();
    return (int)size;
}

void uart0_init(void)
{
    xRxQueue = xQueueCreate(RX_QUEUE_LEN, sizeof(uint8_t));
    xTxMutex = xSemaphoreCreateMutex();

    /* 清空 RX FIFO 残留数据 */
    uint8_t dummy;
    while (DL_UART_receiveDataCheck(UART_0_INST, &dummy)) {}

    NVIC_SetPriority(UART_0_INST_INT_IRQN, configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    DL_UART_enableInterrupt(UART_0_INST, DL_UART_INTERRUPT_RX);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
}

/* ISR：将收到的字节推入 FreeRTOS 队列 */
void UART_0_INST_IRQHandler(void)
{
    DL_UART_IIDX iid = DL_UART_getPendingInterrupt(UART_0_INST);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (iid == DL_UART_IIDX_RX) {
        uint8_t byte = DL_UART_receiveData(UART_0_INST);
        if (xRxQueue != NULL) {
            xQueueSendFromISR(xRxQueue, &byte, &xHigherPriorityTaskWoken);
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* 发送任务：每 500ms 输出计数 + 堆使用率 */
void uart0_Send_task(void *arg)
{
    int n = 0;
    char buf[TX_BUF_SIZE];

    while (1) {
        TickType_t now = xTaskGetTickCount();
        snprintf(buf, sizeof(buf), "Hello UART0 %d [tick=%lu]\r\n", n, (unsigned long)now);
        uart0_sendStr(buf);

        if (n % 10 == 0) {
            size_t freeHeap = xPortGetFreeHeapSize();
            size_t used = configTOTAL_HEAP_SIZE - freeHeap;
            int pct = (used * 100) / configTOTAL_HEAP_SIZE;
            snprintf(buf, sizeof(buf), "[Heap] free:%u/%u (%u%%)\r\n",
                     freeHeap, configTOTAL_HEAP_SIZE, pct);
            uart0_sendStr(buf);
        }
        n++;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* 接收任务：从队列取字节并回显 */
void uart0_Recive_task(void *arg)
{
    uint8_t byte;
    while (1) {
        if (xQueueReceive(xRxQueue, &byte, portMAX_DELAY) == pdTRUE) {
            DL_UART_transmitDataBlocking(UART_0_INST, byte);
        }
    }
}
