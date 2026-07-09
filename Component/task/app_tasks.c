/**
 * @file    app_tasks.c
 * @brief   FreeRTOS 任务、队列、ISR 与对外控制接口胶水层。
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
#include "yaw_control/yaw_control.h"
#include "speed_control/speed_control.h"
#include <stdint.h>
#include <stdbool.h>

#define APP_ENABLE_YAW_DEBUG_PRINT 0  /* 预留调试开关：默认不创建 yaw 串口打印任务 */

/* ═══════════════════════════════════════════════════════════════════════════
 *  数据结构定义
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief MPU6050 姿态消息（通过 attitude_queue 传递）
 * @note  pitch10/roll10/yaw10 单位为 0.1°（放大 10 倍避免浮点传输）
 *        status == 0 表示数据有效，非 0 表示初始化失败
 */
typedef struct {
    int   status;     /* 0=数据有效, 非0=MPU6050 初始化失败 */
    float pitch;    /* 俯仰角 × 10 (°) */
    float roll;     /* 横滚角 × 10 (°) */
    float yaw;      /* 偏航角 × 10 (°) */
} attitude_msg_t;

/**
 * @brief yaw 命令与状态共用结构体
 * @note  同一个结构体用于队列命令、OLED 快照和等待完成判断，避免命令/状态双结构体。
 */
typedef struct {
    int32_t base_speed_rpm;      /* 命令：基准速度 */
    int32_t target_yaw_deg10;    /* 命令：目标 yaw ×10 */
    int32_t current_yaw_deg10;   /* 状态：当前 yaw ×10 */
    int32_t error_yaw_deg10;     /* 状态：目标误差 ×10 */
    int32_t turn_rpm;            /* 状态：yaw 输出差速 */
    bool enabled;                /* 命令/状态：yaw 使能 */
    bool reset_pid;              /* 命令：目标切换后复位 yaw PID */
    bool settled;                /* 状态：已进入死区/保持锁存 */
} yaw_state_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  全局 FreeRTOS 句柄
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── 队列 (Queue): 任务间数据传递 ── */
static QueueHandle_t g_attitude_queue     = NULL;  /* MPU → yaw/OLED */
static QueueHandle_t g_yaw_target_queue   = NULL;  /* 按键/上层接口 → yaw 闭环 */
static QueueHandle_t g_yaw_state_queue    = NULL;  /* yaw 闭环 → OLED/等待接口 */

/* ── 任务句柄 (Task Handle): ISR 中发送任务通知 ── */
static TaskHandle_t g_mpu_task_handle        = NULL;  /* MPU6050 姿态任务 */
static TaskHandle_t g_yaw_loop_task_handle   = NULL;  /* yaw 角闭环任务 */
static TaskHandle_t g_speed_loop_task_handle = NULL;  /* 速度闭环控制任务 */

/* ═══════════════════════════════════════════════════════════════════════════
 *  公开接口实现
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  设置左右轮目标速度（外部接口）
 * @param  left_rpm   左轮目标速度 (RPM)，正前进/负后退
 * @param  right_rpm  右轮目标速度 (RPM)，正前进/负后退
 * @return true=写入成功, false=队列未初始化
 * @note   使用 xQueueOverwrite 确保队列中始终只有最新的目标值
 *         速度闭环任务在下一个 50ms 周期检测到新目标后立即响应
 */
bool app_tasks_set_wheel_speed_target(int32_t left_rpm, int32_t right_rpm)
{
    return speed_control_set_target(left_rpm, right_rpm);
}

bool app_tasks_set_yaw_target(int32_t base_speed_rpm, int32_t target_yaw_deg10)
{
    yaw_state_t target = {0};
    yaw_state_t state = {0};

    if (g_yaw_target_queue == NULL || g_yaw_state_queue == NULL) {
        return false;
    }

    target.base_speed_rpm = base_speed_rpm;
    target.target_yaw_deg10 = yaw_normalize_deg10(target_yaw_deg10);
    target.enabled = true;
    target.reset_pid = true;

    (void)xQueuePeek(g_yaw_state_queue, &state, 0);
    state.base_speed_rpm = target.base_speed_rpm;
    state.target_yaw_deg10 = target.target_yaw_deg10;
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
        yaw_state_t yaw_state = {0};
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
            return 1;  /* 非阻塞轮询：还在等待中 */
        }
        if ((xTaskGetTickCount() - start_tick) >= timeout_ticks) {
            return 0;  /* 超时也结束等待 */
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务 1: LED 闪烁任务 (优先级 1, 栈 128)
 *  ───────────────────────────────────────────
 *  功能: 心跳指示，LED 每 500ms 翻转一次
 *  目的: 视觉确认系统正常运行 (调度器未崩溃)
 * ═══════════════════════════════════════════════════════════════════════════ */
static void led_task(void *pvParameters)
{
    (void)pvParameters;  /* 未使用任务参数 */

    while (1) {
        /* 翻转 LED (PA22), 1Hz 闪烁 */
        led_toggle();
        /* 阻塞 500ms，让出 CPU 给其他任务 */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务 2: MPU6050 姿态采集任务 (优先级 2, 栈 512)
 *  ───────────────────────────────────────────
 *  功能: DMP 姿态数据采集，四元数→欧拉角转换
 *  触发: MPU6050 INT 引脚下降沿 → ISR 发送任务通知
 *
 *  工作流程:
 *  1. 初始化 MPU6050 + DMP (I2C 通信)
 *  2. 等待 DMP 数据就绪中断通知
 *  3. 读取 FIFO → 提取四元数 → 转换为 pitch/roll/yaw
 *  4. 写入 attitude_queue (预留给平衡/巡线任务)
 *
 *  DMP 输出速率: 50Hz (DEFAULT_MPU_HZ = 50)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define MPU_STABLE_REQUIRED_SAMPLES    80U   /* 50Hz 下 80 帧约 1.6s */
#define MPU_STABLE_PITCH_RANGE_DEG     1.50f  /* 整个稳定窗口内 pitch 最大-最小值不超过该值 */
#define MPU_STABLE_ROLL_RANGE_DEG      1.50f  /* 整个稳定窗口内 roll 最大-最小值不超过该值 */
#define MPU_STABLE_YAW_RANGE_DEG       1.00f  /* 整个稳定窗口内 yaw 最大-最小值不超过该值 */

static void mpu_task(void *pvParameters)
{
    (void)pvParameters;
    attitude_msg_t msg = {0};

    /* 上电后先判断 pitch/roll/yaw 三个数据均稳定，再开始写入队列 */
    uint16_t attitude_stable_count = 0;   /* 当前稳定检测窗口内的采样数 */
    float pitch_min = 0.0f;
    float pitch_max = 0.0f;
    float roll_min = 0.0f;
    float roll_max = 0.0f;
    float yaw_min = 0.0f;
    float yaw_max = 0.0f;
    float yaw_window_ref = 0.0f;          /* yaw 窗口参考角，用于处理 ±180° 跨界 */
    float yaw_zero_offset = 0.0f;         /* 复位后 yaw 零点偏移 */
    bool attitude_ready = false;          /* true 后才向队列发布姿态数据 */

    /* ── 等待 200ms: 确保 MPU6050 上电稳定 ── */
    vTaskDelay(pdMS_TO_TICKS(200));

    /* ── 初始化 MPU6050 + DMP ──
     * MPU6050_Init() 执行:
     *   1) I2C 总线恢复 (死锁检测与解除)
     *   2) 发送 DMP 固件 (inv_mpu_dmp_motion_driver.c)
     *   3) 设置传感器方向矩阵
     *   4) 启用 DMP 6 轴四元数推估 + 手势检测
     *   5) 设置 FIFO 输出速率 = 50Hz
     *   6) 启动 DMP
     * 返回 0 = 成功, 非 0 = 失败 */
    msg.status = MPU6050_Init();

    /* ── 初始化失败处理 ──
     * 将错误状态写入队列供其他任务读取，然后永久阻塞
     * 常见失败原因: I2C 总线异常、MPU6050 未应答、DMP 固件加载失败 */
    if (msg.status != 0) {
        xQueueOverwrite(g_attitude_queue, &msg);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));  /* 每秒检查一次(实际不处理) */
        }
    }

    /* ── 清空可能在 DMP 启动期间产生的中断残留 ──
     * ulTaskNotifyTake(pdTRUE, 0): 非阻塞地清空任务通知计数 */
    (void)ulTaskNotifyTake(pdTRUE, 0);

    /* ── 主循环: 等待 DMP 数据就绪中断 ──
     * portMAX_DELAY 表示阻塞直到收到通知 */
    for (;;) {
        /* 阻塞等待 MPU6050 INT 引脚中断通知 (50Hz) */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* 读取 DMP FIFO 数据 → 全局变量 quat[4]
         * Read_Quad() 内部调用 dmp_read_fifo() 取出四元数,
         * 转换为欧拉角存储到全局变量 pitch/roll/yaw (单位: °)
         * 返回 0 = 成功, -1 = FIFO 读取错误, -2 = MPU6050 未就绪 */
        if (Read_Quad() == 0) {
            /* 启动阶段：必须整段窗口内 pitch/roll/yaw 都不继续漂移，才开始写入队列 */
            if (!attitude_ready) {
                if (attitude_stable_count == 0U) {
                    pitch_min = pitch;
                    pitch_max = pitch;
                    roll_min = roll;
                    roll_max = roll;
                    yaw_window_ref = yaw;
                    yaw_min = yaw;
                    yaw_max = yaw;
                    attitude_stable_count = 1U;
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

                attitude_stable_count++;
                if (attitude_stable_count < MPU_STABLE_REQUIRED_SAMPLES) {
                    continue;
                }

                if (((pitch_max - pitch_min) <= MPU_STABLE_PITCH_RANGE_DEG) &&
                    ((roll_max - roll_min) <= MPU_STABLE_ROLL_RANGE_DEG) &&
                    ((yaw_max - yaw_min) <= MPU_STABLE_YAW_RANGE_DEG)) {
                    /* 三个数据稳定后，把当前朝向作为复位 yaw 零点 */
                    yaw_zero_offset = yaw;
                    attitude_ready = true;
                } else {
                    /* 这 10s 内仍在慢慢漂移，重新开启下一轮窗口检测 */
                    attitude_stable_count = 0U;
                    continue;
                }
            }

            msg.status = 0;
            /* pitch/roll 直接使用 DMP 输出，即显示相对重力方向的绝对倾角 */
            msg.pitch = pitch;
            msg.roll  = roll;
            /* yaw 没有磁力计绝对参考，因此只把“复位后的初始朝向”定义为 0° */
            msg.yaw = yaw_normalize_deg(yaw - yaw_zero_offset);

            /* 写入队列（预留给后续平衡/巡线任务消费） */
            xQueueOverwrite(g_attitude_queue, &msg);
        }
        /* 如果 Read_Quad() 失败，跳过本次，等待下一个中断 */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务 3: 设定小车偏航角切换任务 (优先级 2, 栈 192)
 *  ───────────────────────────────────────────
 *  功能: 检测用户按键 (PB21)，循环切换 8 个速度档位
 *  触发: 周期轮询 10ms，软件消抖集成在 key_read_user() 中
 *
 * ═══════════════════════════════════════════════════════════════════════════ */
static void YawKeySet_Task(void *pvParameters)
{
    (void)pvParameters;
    bool key_was = false;        /* 上一次按键状态 */
    int32_t yaw_deg10 = 0;       /* 当前按键档位 yaw，单位 0.1° */

    for (;;) {
        /* 读取当前按键状态 (含软件消抖: 连续两次读到相同电平才确认) */
        bool key_now = key_read_user();

        /* 上升沿检测: 按键从未按下 → 按下 的跳变 */
        if (key_now && !key_was) {
            int32_t base_speed_rpm = 0;
            yaw_deg10 += 450;          /* 每按一次增加 45° */
            if (yaw_deg10 > 1800) {
                yaw_deg10 = 0;
            }

            yaw_state_t state = {0};
            if (g_yaw_state_queue != NULL &&
                xQueuePeek(g_yaw_state_queue, &state, 0) == pdPASS) {
                base_speed_rpm = state.base_speed_rpm;
            }
            (void)app_tasks_set_yaw_target(base_speed_rpm, yaw_deg10);
        }

        key_was = key_now;  /* 保存当前状态用于下次边沿检测 */

        /* 10ms 轮询, 按键响应延迟 ≤ 20ms (2 次轮询) */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务 4: yaw 角闭环任务 (优先级 2, 栈 384)
 *  ───────────────────────────────────────────
 *  功能: 读取 MPU6050 yaw，位置式 PID 计算转向差速，再通过统一轮速接口
 *        app_tasks_set_wheel_speed_target() 下发到已有双轮速度闭环。
 * ═══════════════════════════════════════════════════════════════════════════ */

#define YAW_LOOP_TIMER_TICK_MS      10
#define YAW_LOOP_PERIOD_MS          50
#define YAW_LOOP_DIVIDER            (YAW_LOOP_PERIOD_MS / YAW_LOOP_TIMER_TICK_MS)

static void yaw_loop_task(void *pvParameters)
{
    (void)pvParameters;

    yaw_control_t yaw_control;
    yaw_state_t target = {0};
    yaw_state_t status = {0};
    uint32_t tick_divider = 0;

    yaw_control_init(&yaw_control);

    for (;;) {
        bool do_yaw_control = false;

        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        tick_divider++;
        if (tick_divider >= YAW_LOOP_DIVIDER) {
            tick_divider = 0;
            do_yaw_control = true;
        }

        if (xQueueReceive(g_yaw_target_queue, &target, 0) == pdPASS &&
            target.reset_pid) {
            yaw_control_reset(&yaw_control);
        }

        if (do_yaw_control) {
            attitude_msg_t attitude;

            status.base_speed_rpm = target.base_speed_rpm;
            status.target_yaw_deg10 = target.target_yaw_deg10;
            status.enabled = target.enabled;
            status.reset_pid = false;
            status.settled = false;

            if (!target.enabled) {
                yaw_control_reset(&yaw_control);
                status.turn_rpm = 0;
            } else if (xQueuePeek(g_attitude_queue, &attitude, 0) != pdPASS ||
                       attitude.status != 0) {
                yaw_control_reset(&yaw_control);
                status.turn_rpm = 0;
                (void)speed_control_set_target(0, 0);
            } else {
                yaw_control_output_t control_out;

                status.current_yaw_deg10 = yaw_normalize_deg10(
                    yaw_float_deg_to_deg10(attitude.yaw));
                status.error_yaw_deg10 = yaw_normalize_deg10(
                    target.target_yaw_deg10 - status.current_yaw_deg10);

                yaw_control_update(&yaw_control,
                                   target.target_yaw_deg10,
                                   status.current_yaw_deg10,
                                   &control_out);

                status.turn_rpm = control_out.turn_rpm;
                status.settled = control_out.settled;

                int32_t left_cmd_rpm = target.base_speed_rpm - status.turn_rpm;
                int32_t right_cmd_rpm = target.base_speed_rpm + status.turn_rpm;
                (void)speed_control_set_target_with_ff(left_cmd_rpm,
                                                       right_cmd_rpm,
                                                       control_out.speed_ff_enable);
            }

            if (g_yaw_state_queue != NULL) {
                (void)xQueueOverwrite(g_yaw_state_queue, &status);
            }
        }

        if (g_speed_loop_task_handle != NULL) {
            xTaskNotifyGive(g_speed_loop_task_handle);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务 5: 速度闭环控制任务：编码器采样 -> 速度 PID -> TB6612 PWM
 * ═══════════════════════════════════════════════════════════════════════════ */
static void speed_loop_task(void *pvParameters)
{
    (void)pvParameters;

    speed_loop_context_t speed_ctx;
    speed_target_msg_t new_target;
    bool low_speed_ff_enable = false;

    speed_loop_init(&speed_ctx);

    NVIC_SetPriority(TIMER_0_INST_INT_IRQN, 3);
    NVIC_ClearPendingIRQ(TIMER_0_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
    DL_TimerG_startCounter(TIMER_0_INST);

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (speed_control_receive_target(&new_target)) {
            low_speed_ff_enable = new_target.low_speed_ff_enable;
            speed_loop_set_target(&speed_ctx,
                                  new_target.left_rpm,
                                  new_target.right_rpm);
        }

        speed_loop_update(&speed_ctx, low_speed_ff_enable);

        speed_control_publish_state(&speed_ctx);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务 6: OLED 显示任务 (优先级 1, 栈 512)
 *  ───────────────────────────────────────────
 *  功能: 显示 MPU/yaw 状态
 *  刷新率: ≈ 5Hz
 * ═══════════════════════════════════════════════════════════════════════════ */
static void oled_task(void *pvParameters)
{
    (void)pvParameters;
    attitude_msg_t status = {0};  /* 本地缓存的显示状态 */
    bool attitude_seen = false;      /* MPU 稳定前队列可能约 20s 无数据 */
    uint16_t oled_clear_count = 99;  /* OLED 刷屏计数器 (调试用) */

    /* ── 初始化 OLED (SSD1306 软件 I2C, PA28=SDA, PA31=SCL) ── */
    OLED_Init();
    OLED_Clear();  /* 清屏 */

    OLED_vsprint(0,0,16,"mpu init...");
    OLED_Refresh();  /* 显存 → 屏幕 */
    for (;;) {

        /* MPU 复位后约 20s 才发布稳定姿态；这里不能永久阻塞，否则 OLED 无等待提示。 */
        if (xQueuePeek(g_attitude_queue, &status, pdMS_TO_TICKS(20)) == pdPASS) {
            attitude_seen = true;
        }

        /* ── 刷新 OLED 显示 (16 号字体, 黑底白字) ── */
        oled_clear_count++;
        if (oled_clear_count >= 100) {
            /* 每 100 次刷新 (约 10s) 清屏一次, 避免残影 */
            OLED_Clear();
            oled_clear_count = 0;
        }
        
        if (!attitude_seen) {
            yaw_state_t yaw_state = {0};
            (void)xQueuePeek(g_yaw_state_queue, &yaw_state, 0);
            int32_t tgt_abs = yaw_abs_i32(yaw_state.target_yaw_deg10);
            char tgt_sign = (yaw_state.target_yaw_deg10 < 0) ? '-' : ' ';
            OLED_vsprint(0,0,16,"MPU stabilizing");
            OLED_vsprint(0,16,16,"wait about 20s ");
            OLED_vsprint(0,32,16,"YT:%c%3ld.%1ld %s", tgt_sign,
                         (long)(tgt_abs / 10), (long)(tgt_abs % 10),
                         yaw_state.enabled ? "ON " : "OFF");
            OLED_vsprint(0,48,16,"yaw not ready  ");
        } else if (status.status) {
            OLED_vsprint(0,0,16,"mpu failure    ");
            OLED_vsprint(0,16,16,"yaw target: ---");
            OLED_vsprint(0,32,16,"yaw now   : ---");
            OLED_vsprint(0,48,16,"check MPU6050  ");
        } else {
            yaw_state_t yaw_state = {0};
            (void)xQueuePeek(g_yaw_state_queue, &yaw_state, 0);
            int32_t tgt_abs = yaw_abs_i32(yaw_state.target_yaw_deg10);
            int32_t now_abs = yaw_abs_i32(yaw_float_deg_to_deg10(status.yaw));
            int32_t err_abs = yaw_abs_i32(yaw_state.error_yaw_deg10);
            char tgt_sign = (yaw_state.target_yaw_deg10 < 0) ? '-' : ' ';
            char now_sign = (yaw_float_deg_to_deg10(status.yaw) < 0) ? '-' : ' ';
            char err_sign = (yaw_state.error_yaw_deg10 < 0) ? '-' : ' ';

            OLED_vsprint(0,0,16,"YT:%c%3ld.%1ld %s", tgt_sign,
                         (long)(tgt_abs / 10), (long)(tgt_abs % 10),
                         yaw_state.enabled ? "ON " : "OFF");
            OLED_vsprint(0,16,16,"YN:%c%3ld.%1ld", now_sign,
                         (long)(now_abs / 10), (long)(now_abs % 10));
            OLED_vsprint(0,32,16,"YE:%c%3ld.%1ld", err_sign,
                         (long)(err_abs / 10), (long)(err_abs % 10));
            OLED_vsprint(0,48,16,"B:%4ld T:%4ld",
                         (long)yaw_state.base_speed_rpm,
                         (long)yaw_state.turn_rpm);
        }

        OLED_Refresh();  /* 显存 → 屏幕 */

        /* 100ms 刷新周期 (OLED I2C 传输耗约 30ms, 剩余时间让出 CPU) */
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  调度器启动函数
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief 创建所有 FreeRTOS 任务并启动调度器。 */
void app_tasks_start(void)
{
    /* ── 创建队列 ──
     * 队列长度均为 1, 配合 xQueueOverwrite 使用:
     *   生产者永远能写入 (覆盖旧值), 消费者读取最新的值 */
    g_attitude_queue     = xQueueCreate(1, sizeof(attitude_msg_t));
    g_yaw_target_queue   = xQueueCreate(1, sizeof(yaw_state_t));
    g_yaw_state_queue    = xQueueCreate(1, sizeof(yaw_state_t));

    /* 任何队列创建失败 → 不可恢复错误, 死循环 */
    if (g_attitude_queue == NULL || g_yaw_target_queue == NULL ||
        g_yaw_state_queue == NULL || !speed_control_queue_init()) {
        while (1) {}
    }

    /* yaw 默认安全关闭：基准速度 0、目标 0°，等待按键/上层接口使能 */
    yaw_state_t initial_yaw = {0};
    initial_yaw.reset_pid = true;
    (void)xQueueOverwrite(g_yaw_target_queue, &initial_yaw);
    (void)xQueueOverwrite(g_yaw_state_queue, &initial_yaw);

    /* ── 创建任务 ──
     * xTaskCreate(任务函数, 任务名, 栈深度, 参数, 优先级, 任务句柄) */

    /* LED 心跳: 最低优先级, 最小栈 */
    xTaskCreate(led_task,        "LED",      128, NULL, 1, NULL);

    /* MPU 姿态: 需 I2C 通信栈 + DMP 浮点运算栈, 分配 512 */
    xTaskCreate(mpu_task,        "MPU",      512, NULL, 2, &g_mpu_task_handle);

    /* yaw 角闭环: 由 10ms Timer 通知，先更新轮速目标，再通知速度闭环 */
    xTaskCreate(yaw_loop_task,   "YAW_LOOP", 384, NULL, 4, &g_yaw_loop_task_handle);

    /* 速度闭环: 由 yaw_loop_task 通知，含 PID 计算，分配 512 */
    xTaskCreate(speed_loop_task, "SPD_LOOP", 512, NULL, 3, &g_speed_loop_task_handle);

    /* yaw 目标切换: PB21 每按一次目标角 +45° */
    xTaskCreate(YawKeySet_Task,  "YAW_KEY",  192, NULL, 2, NULL);

    /* OLED 显示: 含 OLED 显存 (128×8=1024字节) + I2C 通信缓冲 */
    xTaskCreate(oled_task,       "OLED",     512, NULL, 1, NULL);


    /* ── 启动 FreeRTOS 调度器 ──
     * 此后 CPU 控制权交给调度器, 本函数不再返回
     * 如果返回, 说明调度器启动失败 (堆内存不足/配置错误) */
    vTaskStartScheduler();

    /* 安全兜底 */
    while (1) {}
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  中断服务例程 (ISR)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  GPIO GROUP1 中断处理
 * @note   复用两个中断源:
 *         1. MPU6050 INT 引脚 (下降沿) → 通知 mpu_task 读取 DMP 数据
 *         2. 右轮编码器 GPIO (双边沿) → 调用 encoder_right_irq_handler() 软件解码
 *
 *         中断优先级: 3 (configLIBRARY_LOWEST_INTERRUPT_PRIORITY)
 *         使用 FROM_ISR 版本 API 进行任务通知, 在 ISR 结束时执行上下文切换
 */
void GROUP1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;  /* 上下文切换标记 */

    /* ── 子中断 1: 右轮编码器 GPIO 双边沿中断 ──
     * 检测 PA25 + PA14 的电平变化, 在 ISR 中执行正交解码查表
     * 更新全局 volatile 变量 g_right_count */
    if (encoder_right_int_is_pending()) {
        encoder_right_irq_handler();
    }

    /* ── 子中断 2: MPU6050 INT 引脚下降沿中断 ──
     * DMP 数据就绪, 通知 mpu_task 从阻塞中唤醒 */
    if (MPU6050_IntIsPending()) {
        MPU6050_IntClear();  /* 清除中断标记 */

        /* 发送任务通知: 解除 mpu_task 的 ulTaskNotifyTake 阻塞
         * 使用 FROM_ISR 版本, 并在必要时请求上下文切换 */
        if (g_mpu_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_mpu_task_handle, &xHigherPriorityTaskWoken);
        }
    }

    /* 如果 ISR 唤醒了一个更高优先级的任务 (如 speed_loop_task),
     * portYIELD_FROM_ISR 会在 ISR 退出时执行上下文切换 */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief  TIMG0 10ms 定时中断处理
 * @note   每 10ms 产生一次中断, 发送任务通知唤醒 yaw_loop_task；
 *         yaw_loop_task 更新目标后再通知 speed_loop_task
 *
 *         中断优先级: 3 (与 GROUP1 同级, 但由 NVIC 优先级分组决定抢占关系)
 *         SysConfig 中 TIMER_0_INST 配置为单次/周期模式 (period = 10ms)
 */
void TIMER_0_INST_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* 检查中断源: 零比较匹配 (CC0 = 0 触发) */
    switch (DL_TimerG_getPendingInterrupt(TIMER_0_INST)) {
    case DL_TIMER_IIDX_ZERO:
        /* 发送任务通知: 先唤醒 yaw_loop_task，yaw 更新目标后再通知 speed_loop_task */
        if (g_yaw_loop_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_yaw_loop_task_handle,
                                   &xHigherPriorityTaskWoken);
        }
        break;
    default:
        break;
    }

    /* 请求上下文切换 (yaw_loop_task 优先级 4 > 当前任务优先级时执行) */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  FreeRTOS 钩子函数 (Hooks)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  栈溢出钩子函数
 * @note   当任务实际使用的栈空间超过分配值时触发
 *         此处实现为死循环，便于调试时通过断点/变量观察定位问题任务
 *         优化建议: 逐步增加栈大小，可通过 uxTaskGetStackHighWaterMark() 监控
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* 调试时在此设断点, 观察 xTask 和 pcTaskName 确定是哪个任务栈溢出 */
    (void)xTask;
    (void)pcTaskName;
    while (1) {}  /* 死循环: 便于调试器捕获 */
}

/**
 * @brief  提供 Idle 任务（空闲任务）的静态内存
 * @note   FreeRTOSConfig.h 中 configSUPPORT_STATIC_ALLOCATION = 1 时必实现
 *         Idle 任务在没有任何其他任务可运行时执行，自动释放已删除任务的内存
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
    StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;                         /* Idle 任务 TCB */
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];  /* Idle 任务栈 */

    *ppxIdleTaskTCBBuffer   = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

/**
 * @brief  提供 Timer 任务（软件定时器服务任务）的静态内存
 * @note   FreeRTOSConfig.h 中 configUSE_TIMERS = 1 时必实现
 *         当前工程未使用软件定时器, 但静态分配下仍需提供内存
 */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
    StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;                           /* Timer 任务 TCB */
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];  /* Timer 任务栈 */

    *ppxTimerTaskTCBBuffer   = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}
