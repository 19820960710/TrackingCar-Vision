/**
 * 应用 FreeRTOS 任务与中断胶水层。
 * main.c 只保留硬件初始化；任务、队列、FreeRTOS 钩子统一放在这里。
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "ti_msp_dl_config.h"
#include "task/app_tasks.h"
#include "led/led.h"
#include "led/key.h"
#include "oled/oled.h"
#include "mpu6050/mpu6050.h"
#include "tb6612/tb6612.h"
#include "encoder/encoder.h"
#include "pid/pid.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int status;
    float pitch10;
    float roll10;
    float yaw10;
} attitude_msg_t;

#define ENCODER_SPEED_PERIOD_MS     10
#define PID_DEFAULT_KP_MILLI        80      /* 实测稳定参数：0.080 */
#define PID_DEFAULT_KI_MILLI        50      /* 实测稳定参数：0.050 */
#define PID_DEFAULT_KD_MILLI        0
#define PID_OUTPUT_MIN              (-50)
#define PID_OUTPUT_MAX              (50)

typedef struct {
    uint32_t seq;
    uint32_t t_ms;
    int32_t target_rpm;
    int32_t left_rpm;
    int32_t right_rpm;
    int32_t left_pwm;
    int32_t right_pwm;
    int8_t gear_index;
} speed_status_msg_t;

static const int32_t g_speed_gears[] = {
    20, 100, 300, 400, -20, -100, -300, -400
};
#define SPEED_GEAR_COUNT ((int)(sizeof(g_speed_gears) / sizeof(g_speed_gears[0])))

static QueueHandle_t g_attitude_queue = NULL;
static QueueHandle_t g_status_queue = NULL;
static QueueHandle_t g_target_speed_queue = NULL;
static TaskHandle_t g_mpu_task_handle = NULL;
static TaskHandle_t g_speed_loop_task_handle = NULL;

static int32_t encoder_delta_to_rpm10_by_period(int32_t delta, uint32_t period_ms)
{
    if (period_ms == 0U) {
        return 0;
    }
    return (int32_t)(((int64_t)delta * 600000) /
                     ((int64_t)ENCODER_COUNTS_PER_REV * period_ms));
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

static void speed_gear_task(void *pvParameters)
{
    (void)pvParameters;
    bool key_was = false;
    int gear_index = -1;

    for (;;) {
        bool key_now = key_read_user();
        if (key_now && !key_was) {
            int32_t target;
            gear_index++;
            if (gear_index >= SPEED_GEAR_COUNT) {
                gear_index = 0;
            }
            target = g_speed_gears[gear_index];
            xQueueOverwrite(g_target_speed_queue, &target);
        }
        key_was = key_now;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void speed_loop_task(void *pvParameters)
{
    (void)pvParameters;
    pid_inc_t left_pid;
    pid_inc_t right_pid;
    speed_status_msg_t status = {0};
    int32_t left_rpm10_filt = 0;
    int32_t right_rpm10_filt = 0;
    int32_t left_delta_sum = 0;
    int32_t right_delta_sum = 0;
    uint32_t speed_sample_count = 0;

    pid_inc_init(&left_pid, PID_DEFAULT_KP_MILLI, PID_DEFAULT_KI_MILLI, PID_DEFAULT_KD_MILLI,
                 PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    pid_inc_init(&right_pid, PID_DEFAULT_KP_MILLI, PID_DEFAULT_KI_MILLI, PID_DEFAULT_KD_MILLI,
                 PID_OUTPUT_MIN, PID_OUTPUT_MAX);

    status.gear_index = -1;
    encoder_reset();
    xQueueOverwrite(g_status_queue, &status);

    NVIC_SetPriority(TIMER_0_INST_INT_IRQN, 3);
    NVIC_ClearPendingIRQ(TIMER_0_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
    DL_TimerG_startCounter(TIMER_0_INST);

    for (;;) {
        encoder_data_t encoder;
        int32_t new_target;
        bool speed_updated = false;

        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xQueueReceive(g_target_speed_queue, &new_target, 0) == pdPASS) {
            status.target_rpm = new_target;
            for (int i = 0; i < SPEED_GEAR_COUNT; i++) {
                if (g_speed_gears[i] == new_target) {
                    status.gear_index = (int8_t)i;
                    break;
                }
            }
            pid_inc_reset(&left_pid);
            pid_inc_reset(&right_pid);
            status.left_pwm = 0;
            status.right_pwm = 0;
            if (new_target == 0) {
                tb6612_stop();
            }
        }

        encoder_get_data(&encoder);
        left_delta_sum += encoder.left_delta;
        right_delta_sum += encoder.right_delta;
        speed_sample_count++;

        /* 10ms 固定读取编码器，50ms 窗口计算速度并更新 PID。 */
        if (speed_sample_count >= 5U) {
            int32_t left_rpm10 = encoder_delta_to_rpm10_by_period(
                left_delta_sum, speed_sample_count * ENCODER_SPEED_PERIOD_MS);
            int32_t right_rpm10 = encoder_delta_to_rpm10_by_period(
                right_delta_sum, speed_sample_count * ENCODER_SPEED_PERIOD_MS);

            left_rpm10_filt += (left_rpm10 - left_rpm10_filt) / 2;
            right_rpm10_filt += (right_rpm10 - right_rpm10_filt) / 2;
            left_delta_sum = 0;
            right_delta_sum = 0;
            speed_sample_count = 0;
            speed_updated = true;
        }

        status.seq++;
        status.t_ms = (uint32_t)xTaskGetTickCount();
        status.left_rpm = left_rpm10_filt / 10;
        status.right_rpm = right_rpm10_filt / 10;

        if (status.target_rpm == 0) {
            tb6612_stop();
            status.left_pwm = 0;
            status.right_pwm = 0;
        } else if (speed_updated) {
            status.left_pwm = pid_inc_compute(&left_pid, status.target_rpm, status.left_rpm);
            status.right_pwm = pid_inc_compute(&right_pid, status.target_rpm, status.right_rpm);
            /* 实测电机通道与物理左右相反：TB6612 A 控制物理右轮，B 控制物理左轮。 */
            tb6612_set_speed((int16_t)status.right_pwm, (int16_t)status.left_pwm);
        }

        xQueueOverwrite(g_status_queue, &status);
    }
}

static void oled_task(void *pvParameters)
{
    (void)pvParameters;
    speed_status_msg_t status = {0};

    OLED_Init();
    OLED_Clear();

    for (;;) {
        speed_status_msg_t new_status;
        if (xQueueReceive(g_status_queue, &new_status, 0) == pdPASS) {
            status = new_status;
        }

        OLED_Clear();
        if (status.gear_index < 0) {
            OLED_ShowString(0, 0, "GEAR: STOP", 16, 1);
        } else {
            OLED_vsprint(0, 0, 16, "GEAR:%d", (int)status.gear_index + 1);
        }
        OLED_vsprint(0, 16, 16, "T:%ld rpm", (long)status.target_rpm);
        OLED_vsprint(0, 32, 16, "L:%ld/%ld", (long)status.left_rpm, (long)status.left_pwm);
        OLED_vsprint(0, 48, 16, "R:%ld/%ld", (long)status.right_rpm, (long)status.right_pwm);
        OLED_Refresh();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_tasks_start(void)
{
    g_attitude_queue = xQueueCreate(1, sizeof(attitude_msg_t));
    g_status_queue = xQueueCreate(1, sizeof(speed_status_msg_t));
    g_target_speed_queue = xQueueCreate(1, sizeof(int32_t));
    if (g_attitude_queue == NULL || g_status_queue == NULL || g_target_speed_queue == NULL) {
        while (1) {}
    }

    xTaskCreate(led_task,        "LED",      128, NULL, 1, NULL);
    xTaskCreate(mpu_task,        "MPU",      512, NULL, 2, &g_mpu_task_handle);
    xTaskCreate(speed_loop_task, "SPD_LOOP", 384, NULL, 3, &g_speed_loop_task_handle);
    xTaskCreate(speed_gear_task, "GEAR",     192, NULL, 2, NULL);
    xTaskCreate(oled_task,       "OLED",     512, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) {}
}

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
        if (g_speed_loop_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_speed_loop_task_handle, &xHigherPriorityTaskWoken);
        }
        break;
    default:
        break;
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
