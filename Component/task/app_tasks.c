/**
 * @file    app_tasks.c
 * @brief   FreeRTOS 任务入口、ISR 分发与应用层 API 转发。
 */

#include "FreeRTOS.h"
#include "task.h"
#include "ti_msp_dl_config.h"
#include "task/app_tasks.h"
#include "led/led.h"
#include "mpu6050/mpu6050.h"
#include "encoder/encoder.h"
#include "control/speed_control.h"
#include "service/attitude_service.h"
#include "service/yaw_loop_service.h"
#include "service/yaw_key_service.h"
#include "service/app_oled_service.h"
#include <stdint.h>
#include <stdbool.h>

static TaskHandle_t g_attitude_task_handle = NULL;
static TaskHandle_t g_yaw_loop_task_handle = NULL;
static TaskHandle_t g_speed_loop_task_handle = NULL;

static void led_task(void *pvParameters)
{
    (void)pvParameters;
    for (;;) {
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void attitude_task(void *pvParameters)
{
    (void)pvParameters;

    vTaskDelay(pdMS_TO_TICKS(200));
    if (!attitude_service_begin()) {
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    (void)ulTaskNotifyTake(pdTRUE, 0);
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        attitude_service_process_sample();
    }
}

static void yaw_loop_task(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        yaw_loop_service_step_10ms();

        if (g_speed_loop_task_handle != NULL) {
            xTaskNotifyGive(g_speed_loop_task_handle);
        }
    }
}

static void speed_loop_task(void *pvParameters)
{
    (void)pvParameters;

    NVIC_SetPriority(TIMER_0_INST_INT_IRQN, 3);
    NVIC_ClearPendingIRQ(TIMER_0_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
    DL_TimerG_startCounter(TIMER_0_INST);

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        speed_control_step_10ms();
    }
}

static void yaw_key_task(void *pvParameters)
{
    (void)pvParameters;
    yaw_key_service_t key_ctx;

    yaw_key_service_init(&key_ctx);
    for (;;) {
        yaw_key_service_step_10ms(&key_ctx);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void oled_task(void *pvParameters)
{
    (void)pvParameters;
    app_oled_service_t oled_ctx;

    app_oled_service_init(&oled_ctx);
    for (;;) {
        app_oled_service_update(&oled_ctx);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

bool app_tasks_set_wheel_speed_target(int32_t left_rpm, int32_t right_rpm)
{
    return speed_control_set_target(left_rpm, right_rpm);
}

bool app_tasks_set_yaw_target(int32_t base_speed_rpm, int32_t target_yaw_deg10)
{
    return yaw_loop_service_set_target(base_speed_rpm, target_yaw_deg10);
}

int app_tasks_wait_yaw_settled(uint32_t timeout_ms)
{
    TickType_t start_tick = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    for (;;) {
        bool done = yaw_loop_service_is_settled() &&
                    speed_control_wheels_stopped_snapshot();

        if (done) {
            return 0;
        }
        if (timeout_ms == 0U) {
            return 1;
        }
        if ((xTaskGetTickCount() - start_tick) >= timeout_ticks) {
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool app_tasks_get_attitude(app_attitude_t *out)
{
    return attitude_service_get(out);
}

bool app_tasks_get_wheel_speed(app_wheel_speed_t *out)
{
    speed_control_state_t speed_state;

    if (out == NULL || !speed_control_get_state(&speed_state)) {
        return false;
    }

    out->left_rpm = speed_state.left_rpm;
    out->right_rpm = speed_state.right_rpm;
    out->left_target_rpm = speed_state.left_target_rpm;
    out->right_target_rpm = speed_state.right_target_rpm;
    out->stopped = speed_state.stopped;
    return true;
}

bool app_tasks_get_yaw_status(app_yaw_status_t *out)
{
    return yaw_loop_service_get_status(out);
}

bool app_tasks_get_vehicle_state(app_vehicle_state_t *out)
{
    app_vehicle_state_t snapshot = {0};

    if (out == NULL) {
        return false;
    }

    *out = snapshot;
    out->attitude_available = app_tasks_get_attitude(&out->attitude);
    out->wheel_speed_available = app_tasks_get_wheel_speed(&out->wheel_speed);
    out->yaw_status_available = app_tasks_get_yaw_status(&out->yaw);
    return (out->attitude_available || out->wheel_speed_available ||
            out->yaw_status_available);
}

void app_tasks_start(void)
{
    if (!attitude_service_init() || !yaw_loop_service_init() ||
        !speed_control_init()) {
        while (1) {}
    }

    xTaskCreate(led_task,        "LED",      128, NULL, 1, NULL);
    xTaskCreate(attitude_task,   "MPU",      512, NULL, 2,
                &g_attitude_task_handle);
    xTaskCreate(yaw_loop_task,   "YAW_LOOP", 384, NULL, 4,
                &g_yaw_loop_task_handle);
    xTaskCreate(speed_loop_task, "SPD_LOOP", 512, NULL, 3,
                &g_speed_loop_task_handle);
    xTaskCreate(yaw_key_task,    "YAW_KEY",  192, NULL, 2, NULL);
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
        if (g_attitude_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_attitude_task_handle,
                                   &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void TIMER_0_INST_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    switch (DL_TimerG_getPendingInterrupt(TIMER_0_INST)) {
    case DL_TIMER_IIDX_ZERO:
        if (g_yaw_loop_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_yaw_loop_task_handle,
                                   &xHigherPriorityTaskWoken);
        }
        break;
    default:
        break;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
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
