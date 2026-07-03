/**
 * M0_Templant_FreeRTOS - MSPM0G3507 FreeRTOS 工程模板
 * main.c: 胶水层，硬件初始化 + 创建 FreeRTOS 任务
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "ti_msp_dl_config.h"
#include "led/led.h"
#include "led/key.h"
#include "UART/uart0.h"
#include "oled/oled.h"
#include "mpu6050/mpu6050.h"
#include "tb6612/tb6612.h"
#include <stdio.h>

typedef struct {
    int status;
    float pitch10;
    float roll10;
    float yaw10;
} attitude_msg_t;

/* 电机状态枚举 */
typedef enum {
    MOTOR_STOP = 0,
    MOTOR_FORWARD,
    MOTOR_BACKWARD,
    MOTOR_LEFT,
    MOTOR_RIGHT,
    MOTOR_STATE_COUNT
} motor_state_t;

static QueueHandle_t g_motor_state_queue = NULL;

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
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

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
    motor_state_t motor_state = MOTOR_STOP;
    uint8_t clear_flag = 0;
    OLED_Init();
    OLED_Clear();

    for (;;) {
        motor_state_t new_state;
        if (xQueueReceive(g_motor_state_queue, &new_state, 0) == pdPASS) {
            motor_state = new_state;
        }

        if (xQueueReceive(g_attitude_queue, &msg, pdMS_TO_TICKS(100)) == pdPASS) {
            clear_flag++;
            if (clear_flag > 10) {
                clear_flag = 0;
                OLED_Clear();
            }

            if (msg.status != 0) {
                OLED_ShowString(0, 0, "MPU ERR", 16, 1);
            } else {
                switch (motor_state) {
                case MOTOR_STOP:     OLED_ShowString(0, 0, "STOP",      16, 1); break;
                case MOTOR_FORWARD:  OLED_ShowString(0, 0, "FWD  70%",  16, 1); break;
                case MOTOR_BACKWARD: OLED_ShowString(0, 0, "REV  70%",  16, 1); break;
                case MOTOR_LEFT:     OLED_ShowString(0, 0, "TURN L",    16, 1); break;
                case MOTOR_RIGHT:    OLED_ShowString(0, 0, "TURN R",    16, 1); break;
                default: break;
                }
            }

            if (msg.status == 0) {
                OLED_vsprint(0, 16, 16, "P:%.2f", msg.pitch10);
                OLED_vsprint(0, 32, 16, "R:%.2f", msg.roll10);
                OLED_vsprint(0, 48, 16, "Y:%.2f", msg.yaw10);
            }
            OLED_Refresh();
        }
    }
}

/* ── TB6612 电机测试任务（PB21 按键切换状态）── */
static void tb6612_test_task(void *pvParameters)
{
    (void)pvParameters;
    motor_state_t state = MOTOR_STOP;
    motor_state_t last = MOTOR_STOP;
    bool key_was = false;

    tb6612_stop();
    vTaskDelay(pdMS_TO_TICKS(500));
    xQueueOverwrite(g_motor_state_queue, &state);

    for (;;) {
        bool key_now = key_read_user();

        if (key_now && !key_was) {
            state = (motor_state_t)(((int)state + 1) % MOTOR_STATE_COUNT);
        }
        key_was = key_now;

        if (state != last) {
            last = state;
            switch (state) {
            case MOTOR_STOP:
                tb6612_stop();
                break;
            case MOTOR_FORWARD:
                tb6612_set_speed(100, 100);
                vTaskDelay(pdMS_TO_TICKS(80));
                tb6612_set_speed(70, 70);
                break;
            case MOTOR_BACKWARD:
                tb6612_set_speed(-100, -100);
                vTaskDelay(pdMS_TO_TICKS(80));
                tb6612_set_speed(-70, -70);
                break;
            case MOTOR_LEFT:
                tb6612_set_speed(-100, 100);
                vTaskDelay(pdMS_TO_TICKS(80));
                tb6612_set_speed(-60, 60);
                break;
            case MOTOR_RIGHT:
                tb6612_set_speed(100, -100);
                vTaskDelay(pdMS_TO_TICKS(80));
                tb6612_set_speed(60, -60);
                break;
            default:
                break;
            }
            xQueueOverwrite(g_motor_state_queue, &state);
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

static void prvSetupHardware(void)
{
    SYSCFG_DL_init();

    /* MPU6050 中断由组件内部适配 SysConfig 生成宏名，main.c 不直接依赖生成宏。 */
    MPU6050_IntEnable();

    led_init();
    key_init();
    uart0_init();
    tb6612_init();
}

int main(void)
{
    prvSetupHardware();

    uart0_sendStr("M0_Templant_FreeRTOS Ready | 80MHz\r\n");

    g_attitude_queue = xQueueCreate(1, sizeof(attitude_msg_t));
    g_motor_state_queue = xQueueCreate(1, sizeof(motor_state_t));
    if (g_attitude_queue == NULL || g_motor_state_queue == NULL) {
        while (1) {}
    }

    xTaskCreate(led_task,          "LED",        128, NULL, 1, NULL);
    xTaskCreate(uart0_Send_task,   "UART_Send",  256, NULL, 1, NULL);
    xTaskCreate(uart0_Recive_task, "UART_Recv",  256, NULL, 1, NULL);
    xTaskCreate(mpu_task,          "MPU",       512, NULL, 2, &g_mpu_task_handle);
    xTaskCreate(oled_task,         "OLED",       512, NULL, 1, NULL);
    xTaskCreate(tb6612_test_task,  "TB6612_TEST",256, NULL, 2, NULL);

    vTaskStartScheduler();

    while (1) {}
}

/* FreeRTOS 钩子 */
void GROUP1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (MPU6050_IntIsPending()) {
        MPU6050_IntClear();
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

