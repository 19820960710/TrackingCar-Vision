/**
 * M0_Templant_FreeRTOS - MSPM0G3507 FreeRTOS 工程模板
 * main.c: 胶水层，硬件初始化 + 创建 FreeRTOS 任务
 */
#include "FreeRTOS.h"
#include "task.h"
#include "ti_msp_dl_config.h"
#include "led/led.h"
#include "UART/uart0.h"
#include "oled/oled.h"
#include <stdio.h>

static void led_task(void *pvParameters)
{
    (void)pvParameters;
    while (1) {
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void oled_task(void *pvParameters)
{
    (void)pvParameters;
    uint32_t cnt = 0;
    char line[32];

    for (;;) {
        OLED_Clear();
        OLED_ShowString(0, 0,  "OLED Test", 16, 1);
        OLED_vsprint(0, 16,16, "cnt:%u", cnt);
        OLED_Refresh();
        cnt++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void prvSetupHardware(void)
{
    SYSCFG_DL_init();
    led_init();
    uart0_init();
    OLED_Init();
}

int main(void)
{
    prvSetupHardware();

    uart0_sendStr("M0_Templant_FreeRTOS Ready | 80MHz\r\n");

    xTaskCreate(led_task,          "LED",        128, NULL, 1, NULL);
    xTaskCreate(uart0_Send_task,   "UART_Send",  256, NULL, 1, NULL);
    xTaskCreate(uart0_Recive_task, "UART_Recv",  256, NULL, 1, NULL);
    xTaskCreate(oled_task,         "OLED",       256, NULL, 2, NULL);

    vTaskStartScheduler();

    while (1) {}
}

/* FreeRTOS 钩子 */
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

