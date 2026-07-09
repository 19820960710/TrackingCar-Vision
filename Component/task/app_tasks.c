/**
 * @file    app_tasks.c
 * @brief   FreeRTOS 任务入口、ISR 分发与应用层 API 转发。
 */

#include "FreeRTOS.h"
#include "task.h"
#include "ti_msp_dl_config.h"
#include "task/app_tasks.h"
#include "led/led.h"
#include "led/key.h"
#include "oled/oled.h"
#include "mpu6050/mpu6050.h"
#include "encoder/encoder.h"
#include "control/speed_control.h"
#include "service/attitude_service.h"
#include "service/yaw_loop_service.h"
#include <stdint.h>
#include <stdbool.h>

static TaskHandle_t g_attitude_task_handle = NULL;
static TaskHandle_t g_yaw_loop_task_handle = NULL;
static TaskHandle_t g_speed_loop_task_handle = NULL;

static int32_t app_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
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
    bool key_was = false;
    int32_t yaw_deg10 = 0;

    for (;;) {
        bool key_now = key_read_user();

        if (key_now && !key_was) {
            int32_t base_speed_rpm = 0;
            app_yaw_status_t state = {0};

            yaw_deg10 += 450;
            if (yaw_deg10 > 1800) {
                yaw_deg10 = 0;
            }

            if (yaw_loop_service_get_status(&state)) {
                base_speed_rpm = state.base_speed_rpm;
            }
            (void)yaw_loop_service_set_target(base_speed_rpm, yaw_deg10);
        }

        key_was = key_now;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void oled_print_yaw_target(const app_yaw_status_t *yaw_state)
{
    int32_t tgt_abs = app_abs_i32(yaw_state->target_yaw_deg10);
    char tgt_sign = (yaw_state->target_yaw_deg10 < 0) ? '-' : ' ';

    OLED_vsprint(0, 32, 16, "YT:%c%3ld.%1ld %s", tgt_sign,
                 (long)(tgt_abs / 10), (long)(tgt_abs % 10),
                 yaw_state->enabled ? "ON " : "OFF");
}

static void oled_task(void *pvParameters)
{
    (void)pvParameters;
    bool attitude_seen = false;
    uint16_t clear_count = 99;

    OLED_Init();
    OLED_Clear();
    OLED_vsprint(0, 0, 16, "mpu init...");
    OLED_Refresh();

    for (;;) {
        app_attitude_t attitude = {0};
        app_yaw_status_t yaw_state = {0};

        if (attitude_service_get(&attitude)) {
            attitude_seen = true;
        }
        (void)yaw_loop_service_get_status(&yaw_state);

        clear_count++;
        if (clear_count >= 100) {
            OLED_Clear();
            clear_count = 0;
        }

        if (!attitude_seen) {
            OLED_vsprint(0, 0, 16, "MPU stabilizing");
            OLED_vsprint(0, 16, 16, "wait about 20s ");
            oled_print_yaw_target(&yaw_state);
            OLED_vsprint(0, 48, 16, "yaw not ready  ");
        } else if (!attitude.valid) {
            OLED_vsprint(0, 0, 16, "mpu failure    ");
            OLED_vsprint(0, 16, 16, "yaw target: ---");
            OLED_vsprint(0, 32, 16, "yaw now   : ---");
            OLED_vsprint(0, 48, 16, "check MPU6050  ");
        } else {
            int32_t tgt_abs = app_abs_i32(yaw_state.target_yaw_deg10);
            int32_t now_abs = app_abs_i32(attitude.yaw_deg10);
            int32_t err_abs = app_abs_i32(yaw_state.error_yaw_deg10);
            char tgt_sign = (yaw_state.target_yaw_deg10 < 0) ? '-' : ' ';
            char now_sign = (attitude.yaw_deg10 < 0) ? '-' : ' ';
            char err_sign = (yaw_state.error_yaw_deg10 < 0) ? '-' : ' ';

            OLED_vsprint(0, 0, 16, "YT:%c%3ld.%1ld %s", tgt_sign,
                         (long)(tgt_abs / 10), (long)(tgt_abs % 10),
                         yaw_state.enabled ? "ON " : "OFF");
            OLED_vsprint(0, 16, 16, "YN:%c%3ld.%1ld", now_sign,
                         (long)(now_abs / 10), (long)(now_abs % 10));
            OLED_vsprint(0, 32, 16, "YE:%c%3ld.%1ld", err_sign,
                         (long)(err_abs / 10), (long)(err_abs % 10));
            OLED_vsprint(0, 48, 16, "B:%4ld T:%4ld",
                         (long)yaw_state.base_speed_rpm,
                         (long)yaw_state.turn_rpm);
        }

        OLED_Refresh();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
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
