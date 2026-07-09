/**
 * @file    app_tasks.c
 * @brief   FreeRTOS 任务入口、ISR 分发与应用层 API 转发。
 *
 * @details 本文件是应用层与 service/control 层之间的胶水层，职责：
 *          1. 创建所有 FreeRTOS 任务并启动调度器；
 *          2. 保存任务句柄，承担 ISR -> 任务通知的分发；
 *          3. 对外暴露统一的 app_tasks_* API，转发到对应 service，
 *             外部调用方无需感知 service 模块名与队列句柄。
 *
 *          分层约定（详见 docs/工程总结.md 第 4 节）：
 *          app_tasks -> service(FreeRTOS 适配) -> control(纯算法) -> pid(通用库)
 *
 *          控制链路节拍（10ms Timer 统一驱动）：
 *          TIMER_0 -> yaw_loop_task -> speed_loop_task
 *          yaw 先更新目标，再通知速度环执行，避免时序错位。
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
#include "service/speed_service.h"
#include "service/attitude_service.h"
#include "service/yaw_loop_service.h"
#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务句柄：只保存需要被 ISR 通知的任务，其余任务句柄不保留。
 * ═══════════════════════════════════════════════════════════════════════════ */
static TaskHandle_t g_attitude_task_handle = NULL;   /* MPU6050 INT 数据就绪通知 */
static TaskHandle_t g_yaw_loop_task_handle = NULL;    /* TIMER_0 10ms 节拍通知 */
static TaskHandle_t g_speed_loop_task_handle = NULL;  /* 由 yaw_loop_task 通知 */

/* 取绝对值；OLED 显示角度/速度时用于符号与数值分离。 */
static int32_t app_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  对外控制 API：转发到 service 层，不启用 yaw 专用低速前馈。
 * ═══════════════════════════════════════════════════════════════════════════ */

/* 设置左右轮目标速度 (RPM)；用于直行/差速转向/停止。 */
bool app_tasks_set_wheel_speed_target(int32_t left_rpm, int32_t right_rpm)
{
    return speed_service_set_target(left_rpm, right_rpm);
}

/* 设置 yaw 闭环目标：base_speed_rpm 为基准速度，target_yaw_deg10 为目标角×10。 */
bool app_tasks_set_yaw_target(int32_t base_speed_rpm, int32_t target_yaw_deg10)
{
    return yaw_loop_service_set_target(base_speed_rpm, target_yaw_deg10);
}

/* 等待 yaw 到位且两轮停止；组合 yaw 状态与速度状态，不要求速度环写 yaw 状态。
 * 返回：0=已到位或超时结束；1=未到位仍在等待（仅 timeout_ms==0 非阻塞轮询时）。 */
int app_tasks_wait_yaw_settled(uint32_t timeout_ms)
{
    TickType_t start_tick = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    for (;;) {
        bool done = yaw_loop_service_is_settled() &&
                    speed_service_wheels_stopped_snapshot();

        if (done) {
            return 0;
        }
        if (timeout_ms == 0U) {          /* 非阻塞轮询：未到位返回 1 */
            return 1;
        }
        if ((xTaskGetTickCount() - start_tick) >= timeout_ticks) {
            return 0;                     /* 超时按已结束处理 */
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  对外状态快照 API：只读 getter，不暴露 PID/PWM/队列等内部状态。
 * ═══════════════════════════════════════════════════════════════════════════ */

/* 获取最新姿态快照；MPU 未稳定时 valid=false。 */
bool app_tasks_get_attitude(app_attitude_t *out)
{
    return attitude_service_get(out);
}

/* 获取两轮速度快照；将 service 内部结构体映射为应用层结构体。 */
bool app_tasks_get_wheel_speed(app_wheel_speed_t *out)
{
    speed_service_state_t speed_state;

    if (out == NULL || !speed_service_get_state(&speed_state)) {
        return false;
    }

    out->left_rpm = speed_state.left_rpm;
    out->right_rpm = speed_state.right_rpm;
    out->left_target_rpm = speed_state.left_target_rpm;
    out->right_target_rpm = speed_state.right_target_rpm;
    out->stopped = speed_state.stopped;
    return true;
}

/* 获取 yaw 闭环状态快照。 */
bool app_tasks_get_yaw_status(app_yaw_status_t *out)
{
    return yaw_loop_service_get_status(out);
}

/* 聚合状态快照：一次性取姿态+轮速+yaw 状态，任一可用即返回 true。 */
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

/* ═══════════════════════════════════════════════════════════════════════════
 *  FreeRTOS 任务入口
 *
 *  控制类任务（attitude/yaw_loop/speed_loop）只做等待/延时/调用 service step；
 *  按键与 OLED 任务逻辑保留在本文件，便于频繁修改。
 * ═══════════════════════════════════════════════════════════════════════════ */

/* LED 心跳任务：500ms 翻转一次，优先级最低。 */
static void led_task(void *pvParameters)
{
    (void)pvParameters;
    for (;;) {
        led_toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* 姿态任务：MPU6050 INT 通知驱动，DMP 读取后送入稳定检测。
 * 复位后等待 pitch/roll/yaw 在稳定窗口内满足阈值才发布姿态，约需 20s。 */
static void attitude_task(void *pvParameters)
{
    (void)pvParameters;

    /* 延迟 200ms 等外设稳定后再初始化 MPU6050。 */
    vTaskDelay(pdMS_TO_TICKS(200));
    if (MPU6050_Init() != 0) {
        /* 初始化失败：发布无效姿态后挂起，避免反复重试 I2C。 */
        attitude_service_publish_invalid();
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    attitude_service_reset();
    (void)ulTaskNotifyTake(pdTRUE, 0);   /* 清除启动期间残留通知 */
    for (;;) {
        /* 阻塞等待 PB4 中断发出的数据就绪通知。 */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (Read_Quad() == 0) {
            /* pitch/roll/yaw 为 Read_Quad 输出的全局变量。 */
            attitude_service_process_sample(pitch, roll, yaw);
        }
    }
}

/* yaw 环任务：TIMER_0 10ms 通知驱动，内部每 50ms 执行一次 yaw PID。
 * yaw 先更新目标并下发左右轮速度，再通知速度环执行，保证时序。 */
static void yaw_loop_task(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        yaw_loop_service_step_10ms();

        /* 每次被唤醒都通知速度环，速度环内部自行分频 50ms。 */
        if (g_speed_loop_task_handle != NULL) {
            xTaskNotifyGive(g_speed_loop_task_handle);
        }
    }
}

/* 速度环任务：启动 TIMER_0 后由 yaw_loop_task 通知驱动。
 * 10ms 采样编码器，50ms 执行速度 PID。 */
static void speed_loop_task(void *pvParameters)
{
    (void)pvParameters;

    /* 优先级 3，需允许 FreeRTOS FromISR API 的逻辑优先级。 */
    NVIC_SetPriority(TIMER_0_INST_INT_IRQN, 3);
    NVIC_ClearPendingIRQ(TIMER_0_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
    DL_TimerG_startCounter(TIMER_0_INST);

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        speed_service_step_10ms();
    }
}

/* PB21 按键任务：10ms 轮询，按下一次切换到下一档 yaw 目标角。
 * 序列：0° -> 45° -> 90° -> 135° -> 180° -> 0°，基准速度沿用当前状态。 */
static void yaw_key_task(void *pvParameters)
{
    (void)pvParameters;
    bool key_was = false;
    int32_t yaw_deg10 = 0;

    for (;;) {
        bool key_now = key_read_user();

        /* 下降沿检测：按下（低电平）且上次未按下。 */
        if (key_now && !key_was) {
            int32_t base_speed_rpm = 0;
            app_yaw_status_t state = {0};

            yaw_deg10 += 450;            /* +45° */
            if (yaw_deg10 > 1800) {      /* 超过 180° 回到 0° */
                yaw_deg10 = 0;
            }

            /* 沿用当前基准速度，避免按键切档时丢掉行走速度。 */
            if (app_tasks_get_yaw_status(&state)) {
                base_speed_rpm = state.base_speed_rpm;
            }
            (void)app_tasks_set_yaw_target(base_speed_rpm, yaw_deg10);
        }

        key_was = key_now;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* OLED 公用行：绘制 yaw 目标行（YT:目标角 ON/OFF），复用于等待/正常两种状态。 */
static void oled_print_yaw_target(const app_yaw_status_t *yaw_state)
{
    int32_t tgt_abs = app_abs_i32(yaw_state->target_yaw_deg10);
    char tgt_sign = (yaw_state->target_yaw_deg10 < 0) ? '-' : ' ';

    OLED_vsprint(0, 32, 16, "YT:%c%3ld.%1ld %s", tgt_sign,
                 (long)(tgt_abs / 10), (long)(tgt_abs % 10),
                 yaw_state->enabled ? "ON " : "OFF");
}

/* OLED 显示任务：软件 I2C 刷屏耗时，故优先级低、200ms 刷新。
 * 三种显示状态：
 *   1. MPU 稳定中（attitude_seen=false）：提示等待约 20s；
 *   2. MPU 失效（valid=false）：提示检查硬件；
 *   3. 正常：显示 YT/YN/YE/B/T 五行 yaw 闭环状态。 */
static void oled_task(void *pvParameters)
{
    (void)pvParameters;
    bool attitude_seen = false;    /* 曾读到过有效姿态，用于区分“稳定中”与“失效” */
    uint16_t clear_count = 99;     /* 初始 99，首帧即清屏一次 */

    OLED_Init();
    OLED_Clear();
    OLED_vsprint(0, 0, 16, "mpu init...");
    OLED_Refresh();

    for (;;) {
        app_attitude_t attitude = {0};
        app_yaw_status_t yaw_state = {0};

        if (app_tasks_get_attitude(&attitude)) {
            attitude_seen = true;
        }
        (void)app_tasks_get_yaw_status(&yaw_state);

        /* 软件 I2C 不自动清残留字符，定期整屏清除防残影。 */
        clear_count++;
        if (clear_count >= 100) {   /* 约每 20s 清屏一次 */
            OLED_Clear();
            clear_count = 0;
        }

        if (!attitude_seen) {
            /* MPU 还在稳定窗口内，尚未发布过有效姿态。 */
            OLED_vsprint(0, 0, 16, "MPU stabilizing");
            OLED_vsprint(0, 16, 16, "wait about 20s ");
            oled_print_yaw_target(&yaw_state);
            OLED_vsprint(0, 48, 16, "yaw not ready  ");
        } else if (!attitude.valid) {
            /* 曾有效但现在失效，提示检查 MPU6050 接线。 */
            OLED_vsprint(0, 0, 16, "mpu failure    ");
            OLED_vsprint(0, 16, 16, "yaw target: ---");
            OLED_vsprint(0, 32, 16, "yaw now   : ---");
            OLED_vsprint(0, 48, 16, "check MPU6050  ");
        } else {
            /* 正常显示：符号与数值分离，角度按 X.X 格式输出。 */
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

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务创建与启动
 * ═══════════════════════════════════════════════════════════════════════════ */
/* 初始化各 service（创建队列、PID、编码器），创建全部任务并启动调度器。 */
void app_tasks_start(void)
{
    /* service 初始化失败则死循环，不启动调度器。 */
    if (!attitude_service_init() || !yaw_loop_service_init() ||
        !speed_service_init()) {
        while (1) {}
    }

    /* 任务表：优先级 数越高越优先，栈按实际占用留余量。 */
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

/* ═══════════════════════════════════════════════════════════════════════════
 *  ISR 分发：只做标志检查 + 任务通知，不在 ISR 中访问 I2C 等阻塞外设。
 * ═══════════════════════════════════════════════════════════════════════════ */
/* GROUP1 复用两个中断源：右轮编码器 GPIO 双边沿 + MPU6050 INT 数据就绪。 */
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

/* TIMER_0 10ms 统一节拍：ZERO 中断唤醒 yaw_loop_task，由 yaw 环再通知速度环。 */
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

/* ═══════════════════════════════════════════════════════════════════════════
 *  FreeRTOS 钩子与静态分配内存回调
 *  当前使用动态分配（configSUPPORT_STATIC_ALLOCATION=1 时仍需提供回调）。
 * ═══════════════════════════════════════════════════════════════════════════ */
/* 栈溢出钩子：死循环便于调试器捕捉。 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    while (1) {}
}

/* 提供空闲任务 TCB 与栈内存（静态分配配置要求）。 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
    StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/* 提供 FreeRTOS 定时器服务任务 TCB 与栈内存（静态分配配置要求）。 */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
    StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
