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
#include "encoder/encoder.h"
#include <stdio.h>

typedef struct {
    int status;
    float pitch10;
    float roll10;
    float yaw10;
} attitude_msg_t;

#define ENCODER_SPEED_PERIOD_MS 10

typedef struct {
    int32_t left_count;
    int32_t right_count;
    int32_t left_delta;
    int32_t right_delta;
    int32_t left_rpm;
    int32_t right_rpm;
} encoder_speed_msg_t;

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
static QueueHandle_t g_encoder_queue = NULL;

static QueueHandle_t g_attitude_queue = NULL;
static TaskHandle_t g_mpu_task_handle = NULL;
static TaskHandle_t g_encoder_speed_task_handle = NULL;

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

    /* 上电后 MPU6050 需要约 50-100ms 稳定，MSPM0 启动太快会读到 ERR */
    vTaskDelay(pdMS_TO_TICKS(200));

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

static int32_t encoder_delta_to_rpm(int32_t delta)
{
    return (int32_t)(((int64_t)delta * 60000) /
                     ((int64_t)ENCODER_COUNTS_PER_REV * ENCODER_SPEED_PERIOD_MS));
}

static void encoder_speed_task(void *pvParameters)
{
    (void)pvParameters;
    encoder_speed_msg_t msg = {0};

    encoder_reset();
    xQueueOverwrite(g_encoder_queue, &msg);

    NVIC_SetPriority(TIMER_0_INST_INT_IRQN, 3);
    NVIC_ClearPendingIRQ(TIMER_0_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
    DL_TimerG_startCounter(TIMER_0_INST);

    for (;;) {
        encoder_data_t encoder;

        /* TIMG12 每 10ms 中断一次，ISR 只通知任务，实际读取和计算放在任务中。 */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        encoder_get_data(&encoder);
        msg.left_count = encoder.left_count;
        msg.right_count = encoder.right_count;
        msg.left_delta = encoder.left_delta;
        msg.right_delta = encoder.right_delta;
        msg.left_rpm = encoder_delta_to_rpm(encoder.left_delta);
        msg.right_rpm = encoder_delta_to_rpm(encoder.right_delta);
        xQueueOverwrite(g_encoder_queue, &msg);
    }
}

static void oled_task(void *pvParameters)
{
    (void)pvParameters;
    motor_state_t motor_state = MOTOR_STOP;
    encoder_speed_msg_t encoder = {0};

    OLED_Init();
    OLED_Clear();

    for (;;) {
        motor_state_t new_state;
        encoder_speed_msg_t new_encoder;

        if (xQueueReceive(g_motor_state_queue, &new_state, 0) == pdPASS) {
            motor_state = new_state;
        }
        if (xQueueReceive(g_encoder_queue, &new_encoder, 0) == pdPASS) {
            encoder = new_encoder;
        }

        OLED_Clear();
        switch (motor_state) {
        case MOTOR_STOP:     OLED_ShowString(0, 0, "STOP",      16, 1); break;
        case MOTOR_FORWARD:  OLED_ShowString(0, 0, "FWD  70%",  16, 1); break;
        case MOTOR_BACKWARD: OLED_ShowString(0, 0, "REV  70%",  16, 1); break;
        case MOTOR_LEFT:     OLED_ShowString(0, 0, "TURN L",    16, 1); break;
        case MOTOR_RIGHT:    OLED_ShowString(0, 0, "TURN R",    16, 1); break;
        default:             OLED_ShowString(0, 0, "UNKNOWN",   16, 1); break;
        }

        OLED_vsprint(0, 16, 16, "L:%ld rpm", (long)encoder.left_rpm);
        OLED_vsprint(0, 32, 16, "R:%ld rpm", (long)encoder.right_rpm);
        OLED_vsprint(0, 48, 16, "C:%ld/%ld", (long)encoder.left_count, (long)encoder.right_count);
        OLED_Refresh();

        vTaskDelay(pdMS_TO_TICKS(100));
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
                tb6612_set_speed(30, 30);
                break;
            case MOTOR_BACKWARD:
                tb6612_set_speed(-100, -100);
                vTaskDelay(pdMS_TO_TICKS(80));
                tb6612_set_speed(-30, -30);
                break;
            case MOTOR_LEFT:
                tb6612_set_speed(100, -100);
                vTaskDelay(pdMS_TO_TICKS(80));
                tb6612_set_speed(20, -20);
                break;
            case MOTOR_RIGHT:
                tb6612_set_speed(-100, 100);
                vTaskDelay(pdMS_TO_TICKS(80));
                tb6612_set_speed(-20, 20);
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
    encoder_init();
}

int main(void)
{
    prvSetupHardware();

    uart0_sendStr("M0_Templant_FreeRTOS Ready | 80MHz\r\n");

    g_attitude_queue = xQueueCreate(1, sizeof(attitude_msg_t));
    g_motor_state_queue = xQueueCreate(1, sizeof(motor_state_t));
    g_encoder_queue = xQueueCreate(1, sizeof(encoder_speed_msg_t));
    if (g_attitude_queue == NULL || g_motor_state_queue == NULL || g_encoder_queue == NULL) {
        while (1) {}
    }

    xTaskCreate(led_task,          "LED",        128, NULL, 1, NULL);
    // xTaskCreate(uart0_Send_task,   "UART_Send",  256, NULL, 1, NULL);
    // xTaskCreate(uart0_Recive_task, "UART_Recv",  256, NULL, 1, NULL);
    xTaskCreate(mpu_task,          "MPU",       512, NULL, 2, &g_mpu_task_handle);
    xTaskCreate(encoder_speed_task,"ENC_SPD",   256, NULL, 2, &g_encoder_speed_task_handle);
    xTaskCreate(oled_task,         "OLED",       512, NULL, 1, NULL);
    xTaskCreate(tb6612_test_task,  "TB6612_TEST",256, NULL, 2, NULL);

    vTaskStartScheduler();

    while (1) {}
}

/*中断服务函数*/
void GROUP1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (encoder_right_int_is_pending()) {
        encoder_right_irq_handler();
    }

    if (MPU6050_IntIsPending()) {
        MPU6050_IntClear();
        if (g_mpu_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_mpu_task_handle, &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void TIMER_0_INST_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    switch (DL_TimerG_getPendingInterrupt(TIMER_0_INST)) {
    case DL_TIMER_IIDX_ZERO:
        if (g_encoder_speed_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_encoder_speed_task_handle, &xHigherPriorityTaskWoken);
        }
        break;
    default:
        break;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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

