/**
 * @file    app_tasks.c
 * @brief   FreeRTOS 任务、队列、ISR 与应用层接口胶水层。
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
#include "encoder/encoder.h"
#include "control/yaw_control.h"
#include "control/speed_control.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int status;      /* 0=有效，非0=MPU 初始化失败 */
    float pitch;     /* ° */
    float roll;      /* ° */
    float yaw;       /* °，复位稳定后相对零点 */
} attitude_msg_t;

typedef struct {
    app_yaw_target_t value;
    bool enabled;
    bool reset_pid;
} yaw_target_msg_t;

static QueueHandle_t g_attitude_queue = NULL;
static QueueHandle_t g_yaw_target_queue = NULL;
static QueueHandle_t g_yaw_state_queue = NULL;

static TaskHandle_t g_mpu_task_handle = NULL;
static TaskHandle_t g_yaw_loop_task_handle = NULL;
static TaskHandle_t g_speed_loop_task_handle = NULL;

/* ───────────────────────────── 对外接口 ───────────────────────────── */

bool app_tasks_set_wheel_speed_target(int32_t left_rpm, int32_t right_rpm)
{
    return speed_control_set_target(left_rpm, right_rpm);
}

bool app_tasks_set_yaw_target(int32_t base_speed_rpm, int32_t target_yaw_deg10)
{
    yaw_target_msg_t target = {0};
    app_yaw_status_t state = {0};

    if (g_yaw_target_queue == NULL || g_yaw_state_queue == NULL) {
        return false;
    }

    target.value.base_speed_rpm = base_speed_rpm;
    target.value.target_yaw_deg10 = yaw_normalize_deg10(target_yaw_deg10);
    target.enabled = true;
    target.reset_pid = true;

    (void)xQueuePeek(g_yaw_state_queue, &state, 0);
    state.base_speed_rpm = target.value.base_speed_rpm;
    state.target_yaw_deg10 = target.value.target_yaw_deg10;
    state.enabled = target.enabled;
    state.settled = false;
    (void)xQueueOverwrite(g_yaw_state_queue, &state);

    return (xQueueOverwrite(g_yaw_target_queue, &target) == pdPASS);
}

int app_tasks_wait_yaw_settled(uint32_t timeout_ms)
{
    TickType_t start_tick = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    for (;;) {
        app_yaw_status_t yaw_state = {0};
        bool done = false;

        if (g_yaw_state_queue != NULL &&
            xQueuePeek(g_yaw_state_queue, &yaw_state, 0) == pdPASS) {
            done = (yaw_state.enabled && yaw_state.settled &&
                    speed_control_wheels_stopped_snapshot());
        }

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
    attitude_msg_t msg = {0};

    if (out == NULL || g_attitude_queue == NULL ||
        xQueuePeek(g_attitude_queue, &msg, 0) != pdPASS) {
        return false;
    }

    out->pitch_deg10 = yaw_float_deg_to_deg10(msg.pitch);
    out->roll_deg10 = yaw_float_deg_to_deg10(msg.roll);
    out->yaw_deg10 = yaw_float_deg_to_deg10(msg.yaw);
    out->valid = (msg.status == 0);
    return true;
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
    if (out == NULL || g_yaw_state_queue == NULL ||
        xQueuePeek(g_yaw_state_queue, out, 0) != pdPASS) {
        return false;
    }
    return true;
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

/* ───────────────────────────── 基础任务 ───────────────────────────── */

static void led_task(void *pvParameters)
{
    (void)pvParameters;
    for (;;) {
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

#define MPU_STABLE_REQUIRED_SAMPLES    80U
#define MPU_STABLE_PITCH_RANGE_DEG     1.50f
#define MPU_STABLE_ROLL_RANGE_DEG      1.50f
#define MPU_STABLE_YAW_RANGE_DEG       1.00f

static void mpu_task(void *pvParameters)
{
    (void)pvParameters;
    attitude_msg_t msg = {0};
    uint16_t stable_count = 0;
    float pitch_min = 0.0f;
    float pitch_max = 0.0f;
    float roll_min = 0.0f;
    float roll_max = 0.0f;
    float yaw_min = 0.0f;
    float yaw_max = 0.0f;
    float yaw_window_ref = 0.0f;
    float yaw_zero_offset = 0.0f;
    bool attitude_ready = false;

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

        if (Read_Quad() != 0) {
            continue;
        }

        if (!attitude_ready) {
            if (stable_count == 0U) {
                pitch_min = pitch;
                pitch_max = pitch;
                roll_min = roll;
                roll_max = roll;
                yaw_window_ref = yaw;
                yaw_min = yaw;
                yaw_max = yaw;
                stable_count = 1U;
                continue;
            }

            float yaw_unwrapped = yaw_window_ref +
                yaw_normalize_deg(yaw - yaw_window_ref);

            if (pitch < pitch_min) pitch_min = pitch;
            if (pitch > pitch_max) pitch_max = pitch;
            if (roll < roll_min) roll_min = roll;
            if (roll > roll_max) roll_max = roll;
            if (yaw_unwrapped < yaw_min) yaw_min = yaw_unwrapped;
            if (yaw_unwrapped > yaw_max) yaw_max = yaw_unwrapped;

            stable_count++;
            if (stable_count < MPU_STABLE_REQUIRED_SAMPLES) {
                continue;
            }

            if (((pitch_max - pitch_min) <= MPU_STABLE_PITCH_RANGE_DEG) &&
                ((roll_max - roll_min) <= MPU_STABLE_ROLL_RANGE_DEG) &&
                ((yaw_max - yaw_min) <= MPU_STABLE_YAW_RANGE_DEG)) {
                yaw_zero_offset = yaw;
                attitude_ready = true;
            } else {
                stable_count = 0U;
                continue;
            }
        }

        msg.status = 0;
        msg.pitch = pitch;
        msg.roll = roll;
        msg.yaw = yaw_normalize_deg(yaw - yaw_zero_offset);
        xQueueOverwrite(g_attitude_queue, &msg);
    }
}

static void YawKeySet_Task(void *pvParameters)
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

            if (g_yaw_state_queue != NULL &&
                xQueuePeek(g_yaw_state_queue, &state, 0) == pdPASS) {
                base_speed_rpm = state.base_speed_rpm;
            }
            (void)app_tasks_set_yaw_target(base_speed_rpm, yaw_deg10);
        }

        key_was = key_now;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ───────────────────────────── 控制环任务 ───────────────────────────── */

#define YAW_LOOP_TIMER_TICK_MS      10
#define YAW_LOOP_PERIOD_MS          50
#define YAW_LOOP_DIVIDER            (YAW_LOOP_PERIOD_MS / YAW_LOOP_TIMER_TICK_MS)

typedef struct {
    yaw_control_t control;
    yaw_target_msg_t target;
    app_yaw_status_t status;
    uint32_t tick_divider;
} yaw_loop_context_t;

static void yaw_loop_init_context(yaw_loop_context_t *ctx)
{
    yaw_loop_context_t init = {0};

    if (ctx == NULL) {
        return;
    }

    *ctx = init;
    yaw_control_init(&ctx->control);
}

static bool yaw_loop_period_elapsed(yaw_loop_context_t *ctx)
{
    ctx->tick_divider++;
    if (ctx->tick_divider < YAW_LOOP_DIVIDER) {
        return false;
    }

    ctx->tick_divider = 0;
    return true;
}

static void yaw_loop_receive_target(yaw_loop_context_t *ctx)
{
    if (xQueueReceive(g_yaw_target_queue, &ctx->target, 0) == pdPASS &&
        ctx->target.reset_pid) {
        yaw_control_reset(&ctx->control);
    }
}

static void yaw_loop_update_50ms(yaw_loop_context_t *ctx)
{
    attitude_msg_t attitude;
    app_yaw_status_t *status = &ctx->status;
    const yaw_target_msg_t *target = &ctx->target;

    status->base_speed_rpm = target->value.base_speed_rpm;
    status->target_yaw_deg10 = target->value.target_yaw_deg10;
    status->enabled = target->enabled;
    status->settled = false;

    if (!target->enabled) {
        yaw_control_reset(&ctx->control);
        status->turn_rpm = 0;
    } else if (xQueuePeek(g_attitude_queue, &attitude, 0) != pdPASS ||
               attitude.status != 0) {
        yaw_control_reset(&ctx->control);
        status->turn_rpm = 0;
        (void)speed_control_set_target(0, 0);
    } else {
        yaw_control_output_t control_out;

        status->current_yaw_deg10 = yaw_normalize_deg10(
            yaw_float_deg_to_deg10(attitude.yaw));
        status->error_yaw_deg10 = yaw_normalize_deg10(
            target->value.target_yaw_deg10 - status->current_yaw_deg10);

        yaw_control_update(&ctx->control,
                           target->value.target_yaw_deg10,
                           status->current_yaw_deg10,
                           &control_out);

        status->turn_rpm = control_out.turn_rpm;
        status->settled = control_out.settled;

        int32_t left_cmd_rpm = target->value.base_speed_rpm - status->turn_rpm;
        int32_t right_cmd_rpm = target->value.base_speed_rpm + status->turn_rpm;
        (void)speed_control_set_target_with_ff(left_cmd_rpm,
                                               right_cmd_rpm,
                                               control_out.speed_ff_enable);
    }

    (void)xQueueOverwrite(g_yaw_state_queue, status);
}

static void yaw_loop_step_10ms(yaw_loop_context_t *ctx)
{
    yaw_loop_receive_target(ctx);
    if (yaw_loop_period_elapsed(ctx)) {
        yaw_loop_update_50ms(ctx);
    }
}

static void yaw_loop_task(void *pvParameters)
{
    (void)pvParameters;
    yaw_loop_context_t ctx;

    yaw_loop_init_context(&ctx);

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        yaw_loop_step_10ms(&ctx);

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

/* ───────────────────────────── OLED 显示任务 ───────────────────────────── */

static void oled_print_yaw_target(const app_yaw_status_t *yaw_state)
{
    int32_t tgt_abs = yaw_abs_i32(yaw_state->target_yaw_deg10);
    char tgt_sign = (yaw_state->target_yaw_deg10 < 0) ? '-' : ' ';

    OLED_vsprint(0, 32, 16, "YT:%c%3ld.%1ld %s", tgt_sign,
                 (long)(tgt_abs / 10), (long)(tgt_abs % 10),
                 yaw_state->enabled ? "ON " : "OFF");
}

static void oled_task(void *pvParameters)
{
    (void)pvParameters;
    attitude_msg_t attitude = {0};
    bool attitude_seen = false;
    uint16_t oled_clear_count = 99;

    OLED_Init();
    OLED_Clear();
    OLED_vsprint(0, 0, 16, "mpu init...");
    OLED_Refresh();

    for (;;) {
        app_yaw_status_t yaw_state = {0};

        if (xQueuePeek(g_attitude_queue, &attitude, pdMS_TO_TICKS(20)) == pdPASS) {
            attitude_seen = true;
        }
        (void)xQueuePeek(g_yaw_state_queue, &yaw_state, 0);

        oled_clear_count++;
        if (oled_clear_count >= 100) {
            OLED_Clear();
            oled_clear_count = 0;
        }

        if (!attitude_seen) {
            OLED_vsprint(0, 0, 16, "MPU stabilizing");
            OLED_vsprint(0, 16, 16, "wait about 20s ");
            oled_print_yaw_target(&yaw_state);
            OLED_vsprint(0, 48, 16, "yaw not ready  ");
        } else if (attitude.status) {
            OLED_vsprint(0, 0, 16, "mpu failure    ");
            OLED_vsprint(0, 16, 16, "yaw target: ---");
            OLED_vsprint(0, 32, 16, "yaw now   : ---");
            OLED_vsprint(0, 48, 16, "check MPU6050  ");
        } else {
            int32_t now_deg10 = yaw_float_deg_to_deg10(attitude.yaw);
            int32_t tgt_abs = yaw_abs_i32(yaw_state.target_yaw_deg10);
            int32_t now_abs = yaw_abs_i32(now_deg10);
            int32_t err_abs = yaw_abs_i32(yaw_state.error_yaw_deg10);
            char tgt_sign = (yaw_state.target_yaw_deg10 < 0) ? '-' : ' ';
            char now_sign = (now_deg10 < 0) ? '-' : ' ';
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

/* ───────────────────────────── 启动与 ISR ───────────────────────────── */

void app_tasks_start(void)
{
    g_attitude_queue = xQueueCreate(1, sizeof(attitude_msg_t));
    g_yaw_target_queue = xQueueCreate(1, sizeof(yaw_target_msg_t));
    g_yaw_state_queue = xQueueCreate(1, sizeof(app_yaw_status_t));

    if (g_attitude_queue == NULL || g_yaw_target_queue == NULL ||
        g_yaw_state_queue == NULL || !speed_control_init()) {
        while (1) {}
    }

    yaw_target_msg_t initial_target = {0};
    app_yaw_status_t initial_status = {0};
    initial_target.reset_pid = true;
    (void)xQueueOverwrite(g_yaw_target_queue, &initial_target);
    (void)xQueueOverwrite(g_yaw_state_queue, &initial_status);

    xTaskCreate(led_task,        "LED",      128, NULL, 1, NULL);
    xTaskCreate(mpu_task,        "MPU",      512, NULL, 2, &g_mpu_task_handle);
    xTaskCreate(yaw_loop_task,   "YAW_LOOP", 384, NULL, 4, &g_yaw_loop_task_handle);
    xTaskCreate(speed_loop_task, "SPD_LOOP", 512, NULL, 3, &g_speed_loop_task_handle);
    xTaskCreate(YawKeySet_Task,  "YAW_KEY",  192, NULL, 2, NULL);
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
