/**
 * M0_Templant_FreeRTOS - MSPM0G3507 FreeRTOS 工程模板
 * main.c: 硬件初始化 + 启动应用任务
 */
#include "ti_msp_dl_config.h"
#include "led/led.h"
#include "led/key.h"
#include "UART/uart0.h"
#include "mpu6050/mpu6050.h"
#include "tb6612/tb6612.h"
#include "encoder/encoder.h"
#include "task/app_tasks.h"

static void prvSetupHardware(void)
{
    SYSCFG_DL_init();

    MPU6050_IntEnable();

    led_init();
    key_init();
    uart0_init();
    tb6612_init();
    encoder_init();
}

int main(void)
{
    prvSetupHardware();

    uart0_sendStr("M0 Speed Closed Loop Ready | key gears | 80MHz\r\n");

    app_tasks_start();

    while (1) {}
}
