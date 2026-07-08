/**
 * @file    app_tasks.c
 * @brief   应用层 FreeRTOS 任务、队列与中断胶水层
 * @note
 *   ── 设计理念 ──
 *   将 main.c 职责限定为"硬件初始化 + 启动调度器"，
 *   所有 FreeRTOS 任务、队列、中断服务例程 (ISR)、钩子函数统一集中在本文件。
 *   任务间通过 FreeRTOS 队列 + 任务通知 (Task Notify) 实现松耦合通信。
 *
 *   ── 任务一览 ──
 *   | 任务名   | 优先级 | 栈大小 | 触发源                   | 功能                         |
 *   |---------|--------|--------|-------------------------|-----------------------------|
 *   | LED     | 1      | 128    | 周期 vTaskDelay(500ms)   | LED 闪烁, 心跳指示            |
 *   | MPU     | 2      | 512    | MPU6050 INT 引脚中断通知   | DMP 姿态数据采集, 四元数→欧拉角 |
 *   | SPD_LOOP| 3(最高)| 384    | TIMG0 10ms 定时中断通知    | 编码器读取 + 增量式 PID 速度闭环 |
 *   | GEAR    | 2      | 192    | 周期 vTaskDelay(10ms)    | 按键检测 + 速度档位循环切换      |
 *   | OLED    | 1      | 512    | 周期 vTaskDelay(100ms)   | 状态信息显示到 OLED 屏         |
 *
 *   ── 数据流 ──
 *   GEAR ──(目标速度)──→ [target_speed_queue] ──→ SPD_LOOP
 *   SPD_LOOP ──(当前状态)──→ [status_queue] ──→ OLED
 *   MPU ──(姿态)──→ [attitude_queue] ──→ (预留给平衡任务)
 *
 *   ── 速度闭环控制链路 ──
 *   编码器(硬件QEI+软件解码) → speed_loop_task(50ms窗口算RPM) → 增量式PID → TB6612 PWM
 *   TIMG0 10ms 定时中断 ──→ TaskNotify → speed_loop_task 解除阻塞
 *
 *   ── PID 参数 (实测整定值) ──
 *   Kp = 0.080, Ki = 0.050, Kd = 0
 *   输出限幅: ±50 (占空比百分比)
 *   适用于 440 线编码器 + 30:1 减速电机
 *
 *   ── 实测注意事项 ──
 *   - TB6612 通道与物理电机左右相反: A 控制物理右轮, B 控制物理左轮
 *   - 编码器映射: PA26/PA27(QEI)对应物理右轮, PA25/PA14(GPIO)对应物理左轮
 *     encoder_get_data() 内部做了交换，使 left_count/right_count 与小车实际左右一致
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
#include "UART/uart0.h"          /* 调试串口 (printf 重定向) */
#include <stdio.h>

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
 * @brief 速度闭环状态快照（通过 status_queue 传递给 OLED 显示）
 */
typedef struct {
    uint32_t seq;              /* 消息序号（调试用） */
    uint32_t t_ms;             /* FreeRTOS 滴答计数（调试用） */
    int32_t  left_target_rpm;  /* 左轮最终目标速度 (RPM) */
    int32_t  right_target_rpm; /* 右轮最终目标速度 (RPM) */
    int32_t  left_setpoint_rpm;  /* 左轮斜坡后内部目标 (RPM) */
    int32_t  right_setpoint_rpm; /* 右轮斜坡后内部目标 (RPM) */
    int32_t  left_rpm;         /* 左轮实测速度 (RPM, 已滤波) */
    int32_t  right_rpm;        /* 右轮实测速度 (RPM, 已滤波) */
    int32_t  left_pwm;         /* 左轮 PID 输出占空比 (-50 ~ 50) */
    int32_t  right_pwm;        /* 右轮 PID 输出占空比 (-50 ~ 50) */
} speed_status_msg_t;

/**
 * @brief yaw 闭环目标消息（通过 yaw_target_queue 传递）
 */
typedef struct {
    int32_t base_speed_rpm;      /* 基准速度 (RPM) */
    int32_t target_yaw_deg10;    /* 目标 yaw ×10 */
    bool enabled;                /* true=使能 yaw 闭环 */
    bool reset_pid;              /* true=目标切换/模式切换后复位 yaw PID */
} yaw_target_msg_t;

/**
 * @brief yaw 闭环运行状态（OLED 显示用）
 */
typedef struct {
    int32_t base_speed_rpm;
    int32_t target_yaw_deg10;
    int32_t current_yaw_deg10;
    int32_t error_yaw_deg10;
    int32_t turn_rpm;
    int32_t left_cmd_rpm;
    int32_t right_cmd_rpm;
    bool enabled;
    bool attitude_valid;
} yaw_status_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  速度档位表
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @var g_speed_gears
 * @brief 速度档位表 (RPM)
 * @note  正数 = 前进，负数 = 后退
 *        档位索引: 0~3 前进档, 4~7 后退档
 *        按键每按一次循环切换到下一档
 */


/* ═══════════════════════════════════════════════════════════════════════════
 *  全局 FreeRTOS 句柄
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── 队列 (Queue): 任务间数据传递 ── */
static QueueHandle_t g_attitude_queue     = NULL;  /* MPU → yaw/OLED */
static QueueHandle_t g_status_queue       = NULL;  /* 速度闭环 → OLED/调试 */
static QueueHandle_t g_target_speed_queue = NULL;  /* yaw/巡线 → 速度闭环 */
static QueueHandle_t g_yaw_target_queue   = NULL;  /* 按键/串口/上位机 → yaw 闭环 */

/* ── yaw 闭环共享状态：按键、串口和调试输出复用同一份目标 ── */
static int32_t g_yaw_base_speed_rpm = 0;
static int32_t g_yaw_target_deg10 = 0;
static bool g_yaw_enabled = false;
static volatile bool g_speed_start_ff_enable = false; /* yaw 层根据误差开关低速前馈 */
static yaw_status_t g_yaw_status = {0};

/* ── 任务句柄 (Task Handle): ISR 中发送任务通知 ── */
static TaskHandle_t g_mpu_task_handle        = NULL;  /* MPU6050 姿态任务 */
static TaskHandle_t g_yaw_loop_task_handle   = NULL;  /* yaw 角闭环任务 */
static TaskHandle_t g_speed_loop_task_handle = NULL;  /* 速度闭环控制任务 */

/* ═══════════════════════════════════════════════════════════════════════════
 *  yaw 闭环目标辅助函数
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief 将 0.1° 角度归一化到 (-1800, 1800]
 */
static int32_t normalize_angle_deg10(int32_t angle_deg10)
{
    while (angle_deg10 > 1800) {
        angle_deg10 -= 3600;
    }
    while (angle_deg10 <= -1800) {
        angle_deg10 += 3600;
    }
    return angle_deg10;
}

static int32_t float_deg_to_deg10(float angle_deg)
{
    if (angle_deg >= 0.0f) {
        return (int32_t)(angle_deg * 10.0f + 0.5f);
    }
    return (int32_t)(angle_deg * 10.0f - 0.5f);
}

static int32_t abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int32_t clamp_i32_local(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value > max_value) {
        return max_value;
    }
    if (value < min_value) {
        return min_value;
    }
    return value;
}

static int32_t milli_to_i32_round_local(int32_t value_milli)
{
    if (value_milli >= 0) {
        return (value_milli + 500) / 1000;
    }
    return (value_milli - 500) / 1000;
}

static void yaw_get_target_snapshot(int32_t *base_speed_rpm,
                                    int32_t *target_yaw_deg10)
{
    taskENTER_CRITICAL();
    if (base_speed_rpm != NULL) {
        *base_speed_rpm = g_yaw_base_speed_rpm;
    }
    if (target_yaw_deg10 != NULL) {
        *target_yaw_deg10 = g_yaw_target_deg10;
    }
    taskEXIT_CRITICAL();
}

static bool yaw_control_publish_state(int32_t base_speed_rpm,
                                      int32_t target_yaw_deg10,
                                      bool enabled,
                                      bool reset_pid)
{
    yaw_target_msg_t target;

    if (g_yaw_target_queue == NULL) {
        return false;
    }

    target.base_speed_rpm = base_speed_rpm;
    target.target_yaw_deg10 = normalize_angle_deg10(target_yaw_deg10);
    target.enabled = enabled;
    target.reset_pid = reset_pid;

    taskENTER_CRITICAL();
    g_yaw_base_speed_rpm = target.base_speed_rpm;
    g_yaw_target_deg10 = target.target_yaw_deg10;
    g_yaw_enabled = target.enabled;
    taskEXIT_CRITICAL();

    return (xQueueOverwrite(g_yaw_target_queue, &target) == pdPASS);
}

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
    app_wheel_speed_target_t target;

    if (g_target_speed_queue == NULL) {
        return false;
    }

    target.left_rpm  = left_rpm;
    target.right_rpm = right_rpm;

    /* xQueueOverwrite: 队列满时覆盖旧值，保证不阻塞调用者 */
    return (xQueueOverwrite(g_target_speed_queue, &target) == pdPASS);
}

bool app_tasks_set_yaw_target(int32_t base_speed_rpm, int32_t target_yaw_deg10)
{
    return yaw_control_publish_state(base_speed_rpm, target_yaw_deg10,
                                     true, true);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  内部辅助函数
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  编码器增量 → RPM（放大 10 倍，避免浮点）
 * @param  delta     编码器脉冲增量
 * @param  period_ms 累计周期 (ms)
 * @return RPM × 10 (rpm10)
 * @note   公式: rpm10 = (delta / ENCODER_COUNTS_PER_REV) / (period_ms / 60000) * 10
 *         化简 → delta * 600000 / (COUNTS_PER_REV * period_ms)
 *         使用 int64_t 中间计算避免溢出
 */
static int32_t encoder_delta_to_rpm10_by_period(int32_t delta, uint32_t period_ms)
{
    if (period_ms == 0U) {
        return 0;
    }
    /* 600000  = 60s/min × 1000ms/s × 10（放大10倍） */
    return (int32_t)(((int64_t)delta * 600000) /
                     ((int64_t)ENCODER_COUNTS_PER_REV * period_ms));
}

/**
 * @brief  按固定步长逼近目标速度，避免阶跃目标造成机械冲击
 */
static int32_t speed_ramp_step(int32_t current, int32_t target, int32_t step)
{
    if (current < target) {
        current += step;
        if (current > target) {
            current = target;
        }
    } else if (current > target) {
        current -= step;
        if (current < target) {
            current = target;
        }
    }

    return current;
}

/**
 * @brief  将角度归一化到 (-180, 180]，用于 yaw 零点相减后的跨界处理
 */
static float normalize_angle_deg(float angle)
{
    while (angle > 180.0f) {
        angle -= 360.0f;
    }
    while (angle <= -180.0f) {
        angle += 360.0f;
    }
    return angle;
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
                    normalize_angle_deg(yaw - yaw_window_ref);

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
            msg.yaw = normalize_angle_deg(yaw - yaw_zero_offset);

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

            yaw_get_target_snapshot(&base_speed_rpm, NULL);
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

#define YAW_LOOP_TIMER_TICK_MS     10
#define YAW_LOOP_PERIOD_MS          50
#define YAW_LOOP_DIVIDER            (YAW_LOOP_PERIOD_MS / YAW_LOOP_TIMER_TICK_MS)
#define YAW_PID_DEFAULT_KP_MILLI    120
#define YAW_PID_DEFAULT_KI_MILLI    0
#define YAW_PID_DEFAULT_KD_MILLI    170
#define YAW_PID_OUTPUT_LIMIT_RPM    200
#define YAW_DYNAMIC_CAP_BASE_RPM    18
#define YAW_DYNAMIC_CAP_ERR_DIV     14
#define YAW_MIN_TURN_RPM            7    /* 死区外给一次明确修正，避免 4RPM 小碎步 */
#define YAW_DEADBAND_DEG10          12   /* 1.2° 内认为到位，配合短接制动抑制末端碎步 */
#define YAW_REACQUIRE_DEG10         32   /* 到位后需超过 3.2° 才重新修正，避免噪声触发末端碎步 */
#define YAW_REACQUIRE_CONFIRM_COUNT 2U   /* 连续 2 个 yaw 周期超出重捕获阈值才重新修正 */
#define YAW_INTEGRAL_ZONE_DEG10     120  /* 12° 内才积分，避免扰动回正中途积累过多 */
#define YAW_INTEGRAL_LIMIT_RPM      0    /* 保持态默认不用积分追尾差，优先换取无超调/无碎步 */
#define YAW_MIN_TURN_ZONE_DEG10     180  /* 18° 内启用连续恢复速度曲线，避免中途停顿 */
#define YAW_TARGET_RAMP_STEP_DEG10  150  /* 目标斜坡步长，单位 0.1°/50ms，45°约 150ms 完成 */
#define YAW_RECOVER_MAX_TURN_RPM    15   /* 小误差恢复曲线最大转向速度，低于大误差动态限幅 */
#define YAW_APPROACH_BRAKE_ZONE_DEG10 120 /* 12° 内若误差正在快速变小，则提前清零并短接制动 */
#define YAW_APPROACH_DERR_DEG10     2    /* 0.2°/50ms 以上认为正在明显靠近目标 */
#define SPEED_START_FF_PWM          18   /* 速度环低速最小 PWM，过大会导致偶发末端小踢动 */

static int32_t yaw_recover_turn_for_error(int32_t abs_err_deg10,
                                          int32_t deadband_deg10,
                                          int32_t zone_deg10,
                                          int32_t min_turn_rpm)
{
    int32_t span;
    int32_t pos;

    if (min_turn_rpm <= 0 || abs_err_deg10 <= deadband_deg10) {
        return 0;
    }
    int32_t max_turn = YAW_RECOVER_MAX_TURN_RPM;
    if (zone_deg10 <= deadband_deg10 || abs_err_deg10 >= zone_deg10) {
        return max_turn;
    }

    span = zone_deg10 - deadband_deg10;
    pos = abs_err_deg10 - deadband_deg10;
    return min_turn_rpm +
        ((max_turn - min_turn_rpm) * pos) / span;
}

static int32_t yaw_target_ramp_step(int32_t current_deg10,
                                    int32_t target_deg10,
                                    int32_t step_deg10)
{
    int32_t delta = normalize_angle_deg10(target_deg10 - current_deg10);

    if (step_deg10 < 0) {
        step_deg10 = -step_deg10;
    }
    if (step_deg10 == 0 || abs_i32(delta) <= step_deg10) {
        return normalize_angle_deg10(target_deg10);
    }

    if (delta > 0) {
        current_deg10 += step_deg10;
    } else {
        current_deg10 -= step_deg10;
    }
    return normalize_angle_deg10(current_deg10);
}

static int32_t yaw_pid_compute_turn(pid_pos_t *pid,
                                    int32_t err_deg10,
                                    bool allow_static_boost,
                                    int32_t *derr_out,
                                    bool *boost_active_out)
{
    if (derr_out != NULL) {
        *derr_out = 0;
    }
    if (boost_active_out != NULL) {
        *boost_active_out = false;
    }

    if (pid == NULL) {
        return 0;
    }

    int32_t abs_err = abs_i32(err_deg10);
    int32_t deadband = YAW_DEADBAND_DEG10;
    int32_t min_turn = YAW_MIN_TURN_RPM;
    int32_t izone = YAW_INTEGRAL_ZONE_DEG10;
    int32_t ilimit = YAW_INTEGRAL_LIMIT_RPM;

    if (deadband < 0) deadband = -deadband;
    if (min_turn < 0) min_turn = -min_turn;
    if (izone < deadband) izone = deadband;
    if (ilimit < 0) ilimit = -ilimit;

    /* 到位死区：0.8° 内直接清零并复位 PID。
     * 超过死区后再给 4RPM 起步修正，并由速度环渐进最小 PWM 推动，
     * 避免原来 1.5° 死区内要等积分爬几秒才动作。 */
    if (abs_err <= deadband) {
        pid_pos_reset(pid);
        return 0;
    }

    int32_t derr = 0;
    if (pid->first_run) {
        pid->first_run = 0U;
    } else {
        derr = err_deg10 - pid->last_err;
    }
    if (derr_out != NULL) {
        *derr_out = derr;
    }

    bool approaching_target =
        ((err_deg10 > 0 && derr < 0) || (err_deg10 < 0 && derr > 0));
    bool approaching_fast = approaching_target &&
        (abs_i32(derr) >= YAW_APPROACH_DERR_DEG10);

    if (abs_err <= izone && pid->ki_milli != 0) {
        int64_t next_integral = (int64_t)pid->integral_milli +
                                (int64_t)pid->ki_milli * err_deg10;
        int32_t int_limit_milli = ilimit * 1000;
        if (next_integral > int_limit_milli) {
            pid->integral_milli = int_limit_milli;
        } else if (next_integral < -int_limit_milli) {
            pid->integral_milli = -int_limit_milli;
        } else {
            pid->integral_milli = (int32_t)next_integral;
        }
    } else {
        /* 大角度时快速泄放积分，防止切换大目标后 windup 残留。 */
        pid->integral_milli /= 2;
    }
    if (approaching_fast && abs_err <= YAW_APPROACH_BRAKE_ZONE_DEG10) {
        /* 扰动回正接近目标时，提前泄放积分，避免积分残留把车推过头。 */
        pid->integral_milli /= 2;
    }

    int64_t output_milli = 0;
    output_milli += (int64_t)pid->kp_milli * err_deg10;
    output_milli += pid->integral_milli;
    output_milli += (int64_t)pid->kd_milli * derr;

    int32_t out_min_milli = pid->out_min * 1000;
    int32_t out_max_milli = pid->out_max * 1000;
    if (output_milli > out_max_milli) {
        output_milli = out_max_milli;
    } else if (output_milli < out_min_milli) {
        output_milli = out_min_milli;
    }

    int32_t output = milli_to_i32_round_local((int32_t)output_milli);

    /*
     * 非线性输出调度：大角度阶跃时限制最大转向速度，避免惯性过冲；
     * 误差变小时动态限幅自动收窄，配合 D 项形成“刹车区”。
     */
    int32_t dynamic_limit = YAW_DYNAMIC_CAP_BASE_RPM +
                            (abs_err / YAW_DYNAMIC_CAP_ERR_DIV);
    if (dynamic_limit < min_turn) {
        dynamic_limit = min_turn;
    }
    if (dynamic_limit > pid->out_max) {
        dynamic_limit = pid->out_max;
    }
    output = clamp_i32_local(output, -dynamic_limit, dynamic_limit);

    int32_t err_sign = (err_deg10 > 0) ? 1 : -1;
    if (approaching_fast && abs_err <= YAW_APPROACH_BRAKE_ZONE_DEG10) {
        /* 接近目标且误差正在快速变小时，不再反向打一脚，直接让速度目标归零。
         * speed_loop 对零目标会短接制动；这样比反向 turn 更不容易冲过头，也不会末端小碎步。 */
        output = 0;
    }

    /*
     * 最小速度补偿：目标斜坡完成后，小误差修正不能低于 MINY。
     * 这里补的是 yaw 输出 turn_rpm，也就是左右轮速度目标差；
     * 不绕过速度环，不直接拍 PWM，因此编码器速度环仍然负责闭环约束。
     */
    int32_t recover_turn = yaw_recover_turn_for_error(abs_err,
                                                       deadband,
                                                       YAW_MIN_TURN_ZONE_DEG10,
                                                       min_turn);
    bool output_same_direction = (output == 0) ||
        ((output > 0 && err_sign > 0) || (output < 0 && err_sign < 0));
    if (allow_static_boost && recover_turn > 0 &&
        abs_err <= YAW_MIN_TURN_ZONE_DEG10 &&
        !(approaching_fast && abs_err <= YAW_APPROACH_BRAKE_ZONE_DEG10) &&
        output_same_direction && abs_i32(output) < recover_turn) {
        output = (err_sign > 0) ? recover_turn : -recover_turn;
        if (boost_active_out != NULL) {
            *boost_active_out = true;
        }
    }

    output = clamp_i32_local(output, pid->out_min, pid->out_max);
    pid->output = output;
    pid->last_err = err_deg10;
    return output;
}

static void yaw_loop_task(void *pvParameters)
{
    (void)pvParameters;

    pid_pos_t yaw_pid;
    yaw_target_msg_t target = {0, 0, false, true};
    yaw_status_t status = {0};
    uint32_t tick_divider = 0;
    int32_t control_target_yaw_deg10 = 0;
    bool control_target_initialized = false;
    bool yaw_settled_latch = false;
    uint8_t yaw_reacquire_count = 0;

    pid_pos_init(&yaw_pid, YAW_PID_DEFAULT_KP_MILLI,
                 YAW_PID_DEFAULT_KI_MILLI,
                 YAW_PID_DEFAULT_KD_MILLI,
                 -YAW_PID_OUTPUT_LIMIT_RPM,
                 YAW_PID_OUTPUT_LIMIT_RPM);

    for (;;) {
        bool do_yaw_control = false;

        /* 由 TIMER_0 10ms 中断通知。本任务先更新上层 yaw 目标，
         * 再通知 speed_loop_task 采样编码器/执行速度环，保证控制顺序为：
         * Timer -> yaw_loop -> app_tasks_set_wheel_speed_target -> speed_loop。
         */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        tick_divider++;
        if (tick_divider >= YAW_LOOP_DIVIDER) {
            tick_divider = 0;
            do_yaw_control = true;
        }

        if (xQueueReceive(g_yaw_target_queue, &target, 0) == pdPASS) {
            if (target.reset_pid) {
                pid_pos_reset(&yaw_pid);
                yaw_settled_latch = false;
                yaw_reacquire_count = 0;
            }
        }

        if (do_yaw_control) {
            attitude_msg_t attitude;

            status.base_speed_rpm = target.base_speed_rpm;
            status.target_yaw_deg10 = target.target_yaw_deg10;
            status.enabled = g_yaw_enabled;
            status.attitude_valid = false;

            if (!target.enabled) {
                pid_pos_reset(&yaw_pid);
                status.turn_rpm = 0;
                status.left_cmd_rpm = 0;
                status.right_cmd_rpm = 0;
                control_target_initialized = false;
                yaw_settled_latch = false;
                yaw_reacquire_count = 0;
                g_speed_start_ff_enable = false;
            } else if (xQueuePeek(g_attitude_queue, &attitude, 0) != pdPASS ||
                       attitude.status != 0) {
                pid_pos_reset(&yaw_pid);
                status.turn_rpm = 0;
                status.left_cmd_rpm = 0;
                status.right_cmd_rpm = 0;
                control_target_initialized = false;
                yaw_settled_latch = false;
                yaw_reacquire_count = 0;
                g_speed_start_ff_enable = false;
                (void)app_tasks_set_wheel_speed_target(0, 0);
            } else {
                status.attitude_valid = true;
                status.current_yaw_deg10 = normalize_angle_deg10(
                    float_deg_to_deg10(attitude.yaw));
                status.error_yaw_deg10 = normalize_angle_deg10(
                    target.target_yaw_deg10 - status.current_yaw_deg10);

                if (!control_target_initialized) {
                    control_target_yaw_deg10 = status.current_yaw_deg10;
                    control_target_initialized = true;
                }
                control_target_yaw_deg10 = yaw_target_ramp_step(
                    control_target_yaw_deg10,
                    target.target_yaw_deg10,
                    YAW_TARGET_RAMP_STEP_DEG10);

                int32_t control_error_yaw_deg10 = normalize_angle_deg10(
                    control_target_yaw_deg10 - status.current_yaw_deg10);
                bool allow_static_boost = (control_target_yaw_deg10 ==
                                           target.target_yaw_deg10);
                int32_t yaw_derr_deg10 = 0;
                int32_t abs_control_error = abs_i32(control_error_yaw_deg10);
                bool yaw_approaching_close = false;

                if (yaw_settled_latch) {
                    if (abs_control_error >= YAW_REACQUIRE_DEG10) {
                        if (yaw_reacquire_count < YAW_REACQUIRE_CONFIRM_COUNT) {
                            yaw_reacquire_count++;
                        }
                        if (yaw_reacquire_count >= YAW_REACQUIRE_CONFIRM_COUNT) {
                            yaw_settled_latch = false;
                            yaw_reacquire_count = 0;
                            pid_pos_reset(&yaw_pid);
                        }
                    } else {
                        yaw_reacquire_count = 0;
                    }
                }

                if (yaw_settled_latch) {
                    /* 已经到位后保持短接制动，不追 1~3° 的尾差，消除偶发小碎步。 */
                    pid_pos_reset(&yaw_pid);
                    status.turn_rpm = 0;
                    yaw_approaching_close = true;
                } else {
                    status.turn_rpm = yaw_pid_compute_turn(&yaw_pid,
                                                           control_error_yaw_deg10,
                                                           allow_static_boost,
                                                           &yaw_derr_deg10,
                                                           NULL);
                    yaw_approaching_close =
                        ((control_error_yaw_deg10 > 0 && yaw_derr_deg10 < 0) ||
                         (control_error_yaw_deg10 < 0 && yaw_derr_deg10 > 0)) &&
                        abs_control_error <= YAW_APPROACH_BRAKE_ZONE_DEG10 &&
                        abs_i32(yaw_derr_deg10) >= YAW_APPROACH_DERR_DEG10;
                    if (abs_control_error <= YAW_DEADBAND_DEG10 ||
                        (status.turn_rpm == 0 && yaw_approaching_close &&
                         abs_control_error < YAW_REACQUIRE_DEG10)) {
                        yaw_settled_latch = true;
                        yaw_reacquire_count = 0;
                        pid_pos_reset(&yaw_pid);
                        status.turn_rpm = 0;
                    }
                }
                /* 误差仍超过死区且需要转向修正时，才允许速度低速前馈；
                 * 接近目标且误差快速变小时关闭 FFS，避免扰动回正后冲过头。 */
                g_speed_start_ff_enable = (!yaw_settled_latch && status.turn_rpm != 0 &&
                    abs_i32(status.error_yaw_deg10) > YAW_DEADBAND_DEG10 &&
                    !yaw_approaching_close);

                /* yaw PID 输出为差速修正量。若实测发现越修越偏，交换这里的正负号。 */
                status.left_cmd_rpm = target.base_speed_rpm - status.turn_rpm;
                status.right_cmd_rpm = target.base_speed_rpm + status.turn_rpm;

                (void)app_tasks_set_wheel_speed_target(status.left_cmd_rpm,
                                                       status.right_cmd_rpm);
            }

            g_yaw_status = status;
        }

        /* yaw_loop_task 完成目标更新后，再唤醒速度闭环任务。 */
        if (g_speed_loop_task_handle != NULL) {
            xTaskNotifyGive(g_speed_loop_task_handle);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务 5: 速度闭环控制任务 (优先级 3 - 最高, 栈 384)
 *  ───────────────────────────────────────────
 *  这是整个系统最核心的控制任务，负责:
 *    1. 编码器数据采集 (10ms 硬件定时器触发)
 *    2. 速度计算 (5 次采样 = 50ms 窗口, 一阶低通滤波)
 *    3. 增量式 PID 控制 (左右轮独立)
 *    4. 电机 PWM 输出 (TB6612)
 *    5. 状态消息发布 (供 OLED 显示)
 *
 *  时序设计:
 *  ┌─────────────────────────────────────────────────────────────┐
 *  │ TIMG0 10ms ──→ ISR通知 ──→ 读取编码器增量 ──→ 累加        │
 *  │ 第5次时(50ms): 计算 RPM → 低通滤波 → PID计算 → 更新PWM    │
 *  └─────────────────────────────────────────────────────────────┘
 *
 *  滤波: 一阶低通 IIR 滤波器
 *    filtered += (raw - filtered) / 2
 *    截止频率 ≈ 采样率 / (2π * α) 其中 α = 0.5
 *
 *  增量式 PID 公式 (位置式输出):
 *    Δu = Kp*(e[k]-e[k-1]) + Ki*e[k] + Kd*(e[k]-2e[k-1]+e[k-2])
 *    u[k] = clamp(u[k-1] + Δu, out_min, out_max)
 *    其中 e[k] = target - measured
 *
 *  TB6612 通道映射 (实测):
 *    电机 A (AO1/AO2) 连接物理右轮 → tb6612_set_speed(right_pwm, left_pwm)
 *    即: 第 1 个参数=物理右轮, 第 2 个参数=物理左轮
 * ═══════════════════════════════════════════════════════════════════════════ */

 /* ═══════════════════════════════════════════════════════════════════════════
 *  速度闭环控制相关常量
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @def ENCODER_SPEED_PERIOD_MS
 * @brief 编码器读取周期 = 10ms
 * @note  TIMG0 定时器每 10ms 产生中断，触发 speed_loop_task 读取编码器增量
 */
#define ENCODER_SPEED_PERIOD_MS     10

/**
 * @def PID_DEFAULT_KP_MILLI
 * @brief 比例系数 × 1000，实际 Kp = 0.080
 * @note  采用千倍整数表示法，避免 MCU 浮点运算开销
 */
#define PID_DEFAULT_KP_MILLI        80

/**
 * @def PID_DEFAULT_KI_MILLI
 * @brief 积分系数 × 1000，实际 Ki = 0.050
 */
#define PID_DEFAULT_KI_MILLI        50

/**
 * @def PID_DEFAULT_KD_MILLI
 * @brief 微分系数 × 1000，实际 Kd = 0（速度环先关闭 D 项，避免放大编码器量化噪声）
 */
#define PID_DEFAULT_KD_MILLI        0

/**
 * @def PID_OUTPUT_MIN / PID_OUTPUT_MAX
 * @brief PID 输出限幅 ±50（PWM 占空比百分比）
 * @note  防止电机电流过大或 PID 积分饱和 (windup)
 */
#define PID_OUTPUT_MIN              (-80)
#define PID_OUTPUT_MAX              (80)

/**
 * @def SPEED_RAMP_STEP_RPM
 * @brief 速度目标斜坡步进，每 50ms 最多变化 100RPM，保证 yaw 差速目标能快速落到速度环
 */
#define SPEED_RAMP_STEP_RPM         100
#define SPEED_START_FF_SETPOINT_RPM 90
#define SPEED_START_FF_ERR_RPM 2
#define SPEED_START_FF_MIN_SETPOINT_RPM 4
#define SPEED_START_FF_FULL_SETPOINT_RPM 24
#define SPEED_START_FF_MIN_PWM 12

static int32_t speed_apply_start_feedforward(int32_t pwm,
                                             int32_t setpoint_rpm,
                                             int32_t measured_rpm)
{
    int32_t ff_pwm = SPEED_START_FF_PWM;
    int32_t sign;

    if (ff_pwm < 0) {
        ff_pwm = -ff_pwm;
    }

    int32_t abs_setpoint = abs_i32(setpoint_rpm);
    if (ff_pwm == 0 || setpoint_rpm == 0 ||
        abs_setpoint > SPEED_START_FF_SETPOINT_RPM ||
        abs_setpoint < SPEED_START_FF_MIN_SETPOINT_RPM ||
        abs_i32(setpoint_rpm - measured_rpm) <= SPEED_START_FF_ERR_RPM) {
        return pwm;
    }
    if (!g_speed_start_ff_enable) {
        return pwm;
    }

    /* 小 turn 命令不要直接打满最小 PWM，否则目标附近会“顶一下-停一下”。
     * 随 setpoint 逐步抬高前馈：很小的尾段修正更柔，大于约 24RPM 才给满 FFS。 */
    if (abs_setpoint < SPEED_START_FF_FULL_SETPOINT_RPM) {
        int32_t ff_span = ff_pwm - SPEED_START_FF_MIN_PWM;
        if (ff_span > 0) {
            ff_pwm = SPEED_START_FF_MIN_PWM +
                (ff_span * abs_setpoint) / SPEED_START_FF_FULL_SETPOINT_RPM;
        }
    }

    sign = (setpoint_rpm > 0) ? 1 : -1;
    if (pwm != 0 && ((pwm > 0 && sign < 0) || (pwm < 0 && sign > 0))) {
        return pwm;
    }
    if (abs_i32(pwm) >= ff_pwm) {
        return pwm;
    }
    return (sign > 0) ? ff_pwm : -ff_pwm;
}

static void speed_loop_task(void *pvParameters)
{
    (void)pvParameters;

    /* ── PID 控制器实例 (左右轮各一个独立控制器) ── */
    pid_inc_t left_pid;
    pid_inc_t right_pid;

    /* ── 状态快照 (最终写入 status_queue 供 OLED 消费) ── */
    speed_status_msg_t status = {0};

    /* ── 滤波后的速度值 (RPM × 10, 一阶低通) ── */
    int32_t left_rpm10_filt  = 0;
    int32_t right_rpm10_filt = 0;

    /* ── 最终目标与斜坡后内部目标，PID 使用 setpoint，避免换档/反向冲击 ── */
    int32_t left_cmd_target_rpm = 0;
    int32_t right_cmd_target_rpm = 0;
    int32_t left_setpoint_rpm = 0;
    int32_t right_setpoint_rpm = 0;

    /* ── 编码器增量累加器 (50ms 窗口内累加 5 次 10ms 采样) ── */
    int32_t  left_delta_sum  = 0;
    int32_t  right_delta_sum = 0;
    uint32_t speed_sample_count = 0;  /* 采样计数器 (0~4) */

    /* ── 初始化左右轮 PID 控制器 ──
     * 参数单位: 千分比 (Kp=80 即 0.080)
     * 输出限幅: ±50 (PWM 占空比 %) */
    pid_inc_init(&left_pid, PID_DEFAULT_KP_MILLI, PID_DEFAULT_KI_MILLI,
                 PID_DEFAULT_KD_MILLI, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    pid_inc_init(&right_pid, PID_DEFAULT_KP_MILLI, PID_DEFAULT_KI_MILLI,
                 PID_DEFAULT_KD_MILLI, PID_OUTPUT_MIN, PID_OUTPUT_MAX);


    /* ── 清零编码器累积计数 ── */
    encoder_reset();

    /* ── 发布初始状态 ── */
    xQueueOverwrite(g_status_queue, &status);

    /* ── 配置并启动 10ms 硬件定时器 (TIMG0) ──
     * 优先级 3: 高于其他 IRQ (GPIO/UART 等), 保证控制周期精度
     * 注意: TIMER_0_INST 的周期已在 SysConfig 中配置为 10ms */
    NVIC_SetPriority(TIMER_0_INST_INT_IRQN, 3);
    NVIC_ClearPendingIRQ(TIMER_0_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
    DL_TimerG_startCounter(TIMER_0_INST);

    /* ═══════════════════════════════════════════════════════════════
     *  主控制循环: 每 10ms 执行一次 (TIMG0 中断触发)
     * ═══════════════════════════════════════════════════════════════ */
    for (;;) {
        encoder_data_t encoder;
        app_wheel_speed_target_t new_target;
        bool speed_updated = false;  /* 标记本次是否进行了 PID 计算 */

        /* ── 阻塞等待 10ms 定时中断通知 ──
         * 这是整个控制循环的节拍器, 保证精确的 10ms 周期 */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* ── 第 1 步: 检查是否有新的目标速度 (来自按键/巡线) ──
         * xQueueReceive(0) = 非阻塞读取, 有新值才取出 */
        if (xQueueReceive(g_target_speed_queue, &new_target, 0) == pdPASS) {
            left_cmd_target_rpm = new_target.left_rpm;
            right_cmd_target_rpm = new_target.right_rpm;
            status.left_target_rpm  = left_cmd_target_rpm;
            status.right_target_rpm = right_cmd_target_rpm;

            /* 仅目标为 0 时立即停止并复位；普通换档/反向交给斜坡过渡，避免输出突变 */
            if (left_cmd_target_rpm == 0 && right_cmd_target_rpm == 0) {
                left_setpoint_rpm = 0;
                right_setpoint_rpm = 0;
                pid_inc_reset(&left_pid);
                pid_inc_reset(&right_pid);
                status.left_pwm  = 0;
                status.right_pwm = 0;
                tb6612_brake();
            }
        }

        /* ── 第 2 步: 读取编码器增量数据 ──
         * encoder_get_data() 内部:
         *   左轮 (物理): QEI 硬件计数器 → update_left_count()
         *   右轮 (物理): GPIO 软件解码 → ISR 中累积 g_right_count
         *   left_delta/right_delta 是本次读取与上次读取之间的增量 */
        encoder_get_data(&encoder);

        /* 累加增量到 50ms 窗口 */
        left_delta_sum  += encoder.left_delta;
        right_delta_sum += encoder.right_delta;
        speed_sample_count++;

        /* ── 第 3 步: 每 5 次 (50ms) 计算速度并更新 PID ──
         * 5 × 10ms = 50ms 控制周期
         * 选择 50ms 而非 10ms 的原因:
         *   440 线编码器在 10ms 内计数太少, 速度量化误差大
         *   50ms 窗口累计更多脉冲, 速度测量更稳定 */
        if (speed_sample_count >= 5U) {
            /* 计算左右轮 RPM × 10 (50ms 窗口) */
            int32_t left_rpm10 = encoder_delta_to_rpm10_by_period(
                left_delta_sum, speed_sample_count * ENCODER_SPEED_PERIOD_MS);
            int32_t right_rpm10 = encoder_delta_to_rpm10_by_period(
                right_delta_sum, speed_sample_count * ENCODER_SPEED_PERIOD_MS);

            /* ── 一阶低通滤波: 平滑速度测量噪声 ──
             * filtered_new = filtered_old + (raw - filtered_old) / 2
             * 等效于: filtered_new = (raw + filtered_old) / 2
             * 截止频率: fc ≈ 50Hz / (2π × 2) ≈ 4Hz (适合电机速度信号) */
            left_rpm10_filt  += (left_rpm10 - left_rpm10_filt) / 2;
            right_rpm10_filt += (right_rpm10 - right_rpm10_filt) / 2;

            /* 重置累加器, 准备下个 50ms 窗口 */
            left_delta_sum  = 0;
            right_delta_sum = 0;
            speed_sample_count = 0;
            speed_updated = true;  /* 标记需执行 PID 计算 */

            /* 速度斜坡：PID 使用内部 setpoint，而不是直接吃最终目标阶跃 */
            left_setpoint_rpm = speed_ramp_step(left_setpoint_rpm,
                                                left_cmd_target_rpm,
                                                SPEED_RAMP_STEP_RPM);
            right_setpoint_rpm = speed_ramp_step(right_setpoint_rpm,
                                                 right_cmd_target_rpm,
                                                 SPEED_RAMP_STEP_RPM);
        }

        /* ── 第 4 步: 更新状态快照 ── */
        status.left_setpoint_rpm = left_setpoint_rpm;
        status.right_setpoint_rpm = right_setpoint_rpm;
        status.left_rpm   = left_rpm10_filt / 10;            /* rpm10 → RPM */
        status.right_rpm  = right_rpm10_filt / 10;

        /* ── 第 5 步: PID 计算并输出电机 PWM ── */
        if (speed_updated) {
            /* 速度已更新: 使用斜坡后 setpoint 执行增量式 PID 计算 */
            status.left_pwm  = pid_inc_compute(&left_pid,
                status.left_setpoint_rpm, status.left_rpm);
            status.right_pwm = pid_inc_compute(&right_pid,
                status.right_setpoint_rpm, status.right_rpm);

            status.left_pwm = speed_apply_start_feedforward(status.left_pwm,
                status.left_setpoint_rpm, status.left_rpm);
            status.right_pwm = speed_apply_start_feedforward(status.right_pwm,
                status.right_setpoint_rpm, status.right_rpm);

            if (status.left_target_rpm == 0 && status.right_target_rpm == 0 &&
                status.left_setpoint_rpm == 0 && status.right_setpoint_rpm == 0) {
                /* 目标为零且斜坡已归零: 短接制动，减少扰动回正后靠惯性冲过头 */
                tb6612_brake();
                status.left_pwm  = 0;
                status.right_pwm = 0;
                pid_inc_reset(&left_pid);
                pid_inc_reset(&right_pid);
            } else {
                /* ── 电机输出 (实测通道映射反转) ──
                 * TB6612 物理接线:
                 *   电机 A (AO1/AO2) → 物理右轮
                 *   电机 B (BO1/BO2) → 物理左轮
                 * 因此 tb6612_set_speed(参数1=物理右轮, 参数2=物理左轮)
                 * 这里传入 right_pwm 到第 1 参数, left_pwm 到第 2 参数 */
                tb6612_set_speed((int16_t)status.right_pwm,
                                 (int16_t)status.left_pwm);
            }
        }
        /* 注意: 如果 speed_updated == false (不足 5 次采样),
         * 则跳过 PID 计算, 维持当前 PWM 输出不变 */

        /* ── 第 6 步: 发布状态到队列 (OLED 任务消费) ── */
        xQueueOverwrite(g_status_queue, &status);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务 5: OLED 显示任务 (优先级 1, 栈 512)
 *  ───────────────────────────────────────────
 *  功能: 从 status_queue 读取速度闭环状态并刷新到 OLED 屏幕
 *  刷新率: ≈ 10Hz (每 100ms 刷新一次)
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
            yaw_status_t yaw_status = g_yaw_status;
            int32_t tgt_abs = abs_i32(yaw_status.target_yaw_deg10);
            char tgt_sign = (yaw_status.target_yaw_deg10 < 0) ? '-' : ' ';
            OLED_vsprint(0,0,16,"MPU stabilizing");
            OLED_vsprint(0,16,16,"wait about 20s ");
            OLED_vsprint(0,32,16,"YT:%c%3ld.%1ld %s", tgt_sign,
                         (long)(tgt_abs / 10), (long)(tgt_abs % 10),
                         yaw_status.enabled ? "ON " : "OFF");
            OLED_vsprint(0,48,16,"yaw not ready  ");
        } else if (status.status) {
            OLED_vsprint(0,0,16,"mpu failure    ");
            OLED_vsprint(0,16,16,"yaw target: ---");
            OLED_vsprint(0,32,16,"yaw now   : ---");
            OLED_vsprint(0,48,16,"check MPU6050  ");
        } else {
            yaw_status_t yaw_status = g_yaw_status;
            int32_t tgt_abs = abs_i32(yaw_status.target_yaw_deg10);
            int32_t now_abs = abs_i32(float_deg_to_deg10(status.yaw));
            int32_t err_abs = abs_i32(yaw_status.error_yaw_deg10);
            char tgt_sign = (yaw_status.target_yaw_deg10 < 0) ? '-' : ' ';
            char now_sign = (float_deg_to_deg10(status.yaw) < 0) ? '-' : ' ';
            char err_sign = (yaw_status.error_yaw_deg10 < 0) ? '-' : ' ';

            OLED_vsprint(0,0,16,"YT:%c%3ld.%1ld %s", tgt_sign,
                         (long)(tgt_abs / 10), (long)(tgt_abs % 10),
                         yaw_status.enabled ? "ON " : "OFF");
            OLED_vsprint(0,16,16,"YN:%c%3ld.%1ld", now_sign,
                         (long)(now_abs / 10), (long)(now_abs % 10));
            OLED_vsprint(0,32,16,"YE:%c%3ld.%1ld", err_sign,
                         (long)(err_abs / 10), (long)(err_abs % 10));
            OLED_vsprint(0,48,16,"B:%4ld T:%4ld",
                         (long)yaw_status.base_speed_rpm,
                         (long)yaw_status.turn_rpm);
        }

        OLED_Refresh();  /* 显存 → 屏幕 */

        /* 100ms 刷新周期 (OLED I2C 传输耗约 30ms, 剩余时间让出 CPU) */
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}


static void debug_print(void *pvParameters)
{
    (void)pvParameters;
    char buf[128];

    for (;;) {
        attitude_msg_t new_status;

        if (xQueuePeek(g_attitude_queue, &new_status, 0) == pdPASS) {
            snprintf(buf, sizeof(buf), "%.2f\r\n",
                     (float)new_status.yaw);
            uart0_sendStr(buf);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
/* ═══════════════════════════════════════════════════════════════════════════
 *  调度器启动函数
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  创建所有 FreeRTOS 任务并启动调度器
 * @note   执行顺序:
 *         1. 创建 3 个队列 (大小均为 1, 覆盖模式)
 *         2. 创建 5 个任务 (LED/MPU/SPD_LOOP/GEAR/OLED)
 *         3. 调用 vTaskStartScheduler() 启动调度器
 *         4. 调度器启动后不再返回; 若返回则进入死循环 (任务创建失败)
 *
 *         任务优先级分配依据:
 *         - SPD_LOOP (3): 最高, 保证控制周期不受其他任务干扰
 *         - MPU/GEAR (2): 与控制任务配合, 但可被抢占
 *         - LED/OLED (1): 最低, 仅显示功能, 延迟不影响控制
 *
 *         栈空间分配依据 (FreeRTOS 栈单位为 32-bit word):
 *         - 1 word = 4 bytes
 *         - 考虑函数调用深度 + 局部变量 + ISR 嵌套栈
 *         - OLED (512) 和 MPU (512) 最大 (含 sprintf/I2C 通信栈)
 *         - 任务栈溢出时触发 vApplicationStackOverflowHook
 */
void app_tasks_start(void)
{
    /* ── 创建队列 ──
     * 队列长度均为 1, 配合 xQueueOverwrite 使用:
     *   生产者永远能写入 (覆盖旧值), 消费者读取最新的值 */
    g_attitude_queue     = xQueueCreate(1, sizeof(attitude_msg_t));
    g_status_queue       = xQueueCreate(1, sizeof(speed_status_msg_t));
    g_target_speed_queue = xQueueCreate(1, sizeof(app_wheel_speed_target_t));
    g_yaw_target_queue   = xQueueCreate(1, sizeof(yaw_target_msg_t));

    /* 任何队列创建失败 → 不可恢复错误, 死循环 */
    if (g_attitude_queue == NULL || g_status_queue == NULL ||
        g_target_speed_queue == NULL || g_yaw_target_queue == NULL) {
        while (1) {}
    }

    /* yaw 默认安全关闭：基准速度 0、目标 0°，等待按键/串口 START 后使能 */
    (void)yaw_control_publish_state(0, 0, false, true);

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

    xTaskCreate(debug_print,     "DEBUG",    512, NULL, 1, NULL);

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
