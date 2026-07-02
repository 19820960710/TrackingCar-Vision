/**
 * M0_Templant_FreeRTOS - MSPM0G3507 FreeRTOS 工程模板
 * main.c: 胶水层，硬件初始化 + 创建 FreeRTOS 任务
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "ti_msp_dl_config.h"
#include "led/led.h"
#include "UART/uart0.h"
#include "oled/oled.h"
#include "mpu6050/mpu6050.h"
#include <stdio.h>

typedef struct {
    int status;
    float pitch10;
    float roll10;
    float yaw10;
} attitude_msg_t;

static QueueHandle_t g_attitude_queue = NULL;
static TaskHandle_t g_mpu_task_handle = NULL;

static int angle_to_tenth(float angle)
{
    return (angle >= 0.0f) ? (int)(angle * 10.0f + 0.5f) : (int)(angle * 10.0f - 0.5f);
}

static void led_task(void *pvParameters)
{
    (void)pvParameters;
    while (1) {
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void mpu_task(void *pvParameters)
{
    (void)pvParameters;
    attitude_msg_t msg = {0};

    msg.status = MPU6050_Init();
    if (msg.status != 0) {
        xQueueOverwrite(g_attitude_queue, &msg);
        uart0_sendStr("MPU6050 init failed\r\n");
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    uart0_sendStr("MPU6050 init success\r\n");
    (void)ulTaskNotifyTake(pdTRUE, 0);

    for (;;) {
        /* MPU6050 INT(PB4) 到来后再读 FIFO；ISR 只通知任务，不在中断里访问 I2C。 */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (Read_Quad() == 0) {
            msg.status = 0;
            msg.pitch10 = pitch;
            msg.roll10 = roll;
            msg.yaw10 = yaw;
            xQueueOverwrite(g_attitude_queue, &msg);
        }
    }
}

static void oled_task(void *pvParameters)
{
    (void)pvParameters;
    attitude_msg_t msg;
    uint8_t oled_clear_flag = 0;
    OLED_Init();
    OLED_Clear();

    for (;;) {
        if (xQueueReceive(g_attitude_queue, &msg, portMAX_DELAY) == pdPASS) {
            if (msg.status != 0) {
                OLED_Clear();
                OLED_ShowString(0, 0, "MPU6050 ERR", 16, 1);
                OLED_Refresh();
                continue;
            }
            oled_clear_flag++;
            if (oled_clear_flag > 10) {
                oled_clear_flag = 0;
                OLED_Clear();
            }
            OLED_ShowString(0, 0, "MPU6050 DMP", 16, 1);
            OLED_vsprint(0, 16, 16, "P:%.2f", msg.pitch10 );
            OLED_vsprint(0, 32, 16, "R:%.2f", msg.roll10);
            OLED_vsprint(0, 48, 16, "Y:%.2f", msg.yaw10);
            OLED_Refresh();
        }
    }
}

static void prvSetupHardware(void)
{
    SYSCFG_DL_init();
    led_init();
    uart0_init();
}

int main(void)
{
    prvSetupHardware();

    uart0_sendStr("M0_Templant_FreeRTOS Ready | 80MHz\r\n");

    g_attitude_queue = xQueueCreate(1, sizeof(attitude_msg_t));
    if (g_attitude_queue == NULL) {
        while (1) {}
    }

    xTaskCreate(led_task,          "LED",        128, NULL, 1, NULL);
    xTaskCreate(uart0_Send_task,   "UART_Send",  256, NULL, 1, NULL);
    xTaskCreate(uart0_Recive_task, "UART_Recv",  256, NULL, 1, NULL);
    xTaskCreate(mpu_task,          "MPU",       512, NULL, 1, &g_mpu_task_handle);
    xTaskCreate(oled_task,         "OLED",       512, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) {}
}

/* FreeRTOS 钩子 */
void GROUP1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (DL_GPIO_getPendingInterrupt(GPIO_MPU6050_INT_PORT) == GPIO_MPU6050_INT_IIDX) {
        DL_GPIO_clearInterruptStatus(GPIO_MPU6050_INT_PORT, GPIO_MPU6050_INT_PIN);
        if (g_mpu_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_mpu_task_handle, &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask; (void)pcTaskName;
    while (1) {}
}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
    StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
    StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

