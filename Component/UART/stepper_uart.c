/**
 * @file stepper_uart.c
 * @brief 两路独立 ZDT X42S UART 的队列、互斥锁和中断适配。
 */
#include "UART/stepper_uart.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "ti_msp_dl_config.h"

#define STEPPER_UART_RX_QUEUE_LENGTH 64U

typedef struct {
    QueueHandle_t rx_queue;
    SemaphoreHandle_t tx_mutex;
} stepper_uart_context_t;

static stepper_uart_context_t g_uart[STEPPER_UART_COUNT];

static bool axis_is_valid(stepper_uart_axis_t axis)
{
    return (uint32_t)axis < (uint32_t)STEPPER_UART_COUNT;
}

static UART_Regs *uart_instance(stepper_uart_axis_t axis)
{
    return (axis == STEPPER_UART_PITCH) ? UART_PITCH_INST : UART_YAW_INST;
}

static IRQn_Type uart_irqn(stepper_uart_axis_t axis)
{
    return (axis == STEPPER_UART_PITCH) ? UART_PITCH_INST_INT_IRQN :
                                          UART_YAW_INST_INT_IRQN;
}

static void lock_tx(stepper_uart_axis_t axis)
{
    if ((g_uart[axis].tx_mutex != NULL) &&
        (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)) {
        (void)xSemaphoreTake(g_uart[axis].tx_mutex, portMAX_DELAY);
    }
}

static void unlock_tx(stepper_uart_axis_t axis)
{
    if ((g_uart[axis].tx_mutex != NULL) &&
        (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)) {
        (void)xSemaphoreGive(g_uart[axis].tx_mutex);
    }
}

static bool init_axis(stepper_uart_axis_t axis)
{
    uint8_t discarded;
    UART_Regs *instance = uart_instance(axis);

    g_uart[axis].rx_queue =
        xQueueCreate(STEPPER_UART_RX_QUEUE_LENGTH, sizeof(uint8_t));
    g_uart[axis].tx_mutex = xSemaphoreCreateMutex();
    if ((g_uart[axis].rx_queue == NULL) ||
        (g_uart[axis].tx_mutex == NULL)) {
        return false;
    }
    while (DL_UART_receiveDataCheck(instance, &discarded)) {
    }
    NVIC_SetPriority(uart_irqn(axis), configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    DL_UART_enableInterrupt(instance, DL_UART_INTERRUPT_RX);
    NVIC_EnableIRQ(uart_irqn(axis));
    return true;
}

bool stepper_uart_init(void)
{
    return init_axis(STEPPER_UART_YAW) && init_axis(STEPPER_UART_PITCH);
}

bool stepper_uart_write(stepper_uart_axis_t axis,
                        const uint8_t *data,
                        size_t length)
{
    size_t index;
    UART_Regs *instance;

    if (!axis_is_valid(axis) || ((data == NULL) && (length != 0U))) {
        return false;
    }
    instance = uart_instance(axis);
    lock_tx(axis);
    for (index = 0U; index < length; index++) {
        DL_UART_transmitDataBlocking(instance, data[index]);
    }
    unlock_tx(axis);
    return true;
}

bool stepper_uart_read_byte(stepper_uart_axis_t axis,
                            uint8_t *byte,
                            TickType_t timeout_ticks)
{
    if (!axis_is_valid(axis) || (byte == NULL) ||
        (g_uart[axis].rx_queue == NULL) ||
        (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)) {
        return false;
    }
    return xQueueReceive(g_uart[axis].rx_queue, byte, timeout_ticks) == pdTRUE;
}

void stepper_uart_flush_rx(stepper_uart_axis_t axis)
{
    uint8_t byte;

    if (axis_is_valid(axis) && (g_uart[axis].rx_queue != NULL)) {
        while (xQueueReceive(g_uart[axis].rx_queue, &byte, 0U) == pdTRUE) {
        }
    }
}

static void receive_isr(stepper_uart_axis_t axis)
{
    BaseType_t task_woken = pdFALSE;
    UART_Regs *instance = uart_instance(axis);
    uint8_t byte;

    if (DL_UART_getPendingInterrupt(instance) == DL_UART_IIDX_RX) {
        while (DL_UART_receiveDataCheck(instance, &byte)) {
            if (g_uart[axis].rx_queue != NULL) {
                (void)xQueueSendFromISR(g_uart[axis].rx_queue, &byte,
                                        &task_woken);
            }
        }
    }
    portYIELD_FROM_ISR(task_woken);
}

void UART_YAW_INST_IRQHandler(void)
{
    receive_isr(STEPPER_UART_YAW);
}

void UART_PITCH_INST_IRQHandler(void)
{
    receive_isr(STEPPER_UART_PITCH);
}
