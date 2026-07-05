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
#include "UART/uart0.h"          /* 调试串口 (printf 重定向 + 收发双任务) */
#include "stdio.h"





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
    float pitch10;    /* 俯仰角 × 10 (°) */
    float roll10;     /* 横滚角 × 10 (°) */
    float yaw10;      /* 偏航角 × 10 (°) */
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
    int8_t   gear_index;       /* 当前档位索引 (-1=停转/自由模式, 0~7=8个档位) */
} speed_status_msg_t;

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
static const int32_t g_speed_gears[] = {
    20,    /* 档位 1: 极慢速前进 */
    100,   /* 档位 2: 慢速前进   */
    300,   /* 档位 3: 中速前进   */
    400,   /* 档位 4: 快速前进   */
    -20,   /* 档位 5: 极慢速后退 */
    -100,  /* 档位 6: 慢速后退   */
    -300,  /* 档位 7: 中速后退   */
    -400,  /* 档位 8: 快速后退   */
};

/** @brief 档位数量（自动计算，当前为 8） */
#define SPEED_GEAR_COUNT ((int)(sizeof(g_speed_gears) / sizeof(g_speed_gears[0])))

/* ═══════════════════════════════════════════════════════════════════════════
 *  全局 FreeRTOS 句柄
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── 队列 (Queue): 任务间数据传递 ── */
static QueueHandle_t g_attitude_queue     = NULL;  /* MPU → 平衡任务 (预留) */
static QueueHandle_t g_status_queue       = NULL;  /* 速度闭环 → OLED */
static QueueHandle_t g_target_speed_queue = NULL;  /* 按键/巡线 → 速度闭环 */

/* ── 任务句柄 (Task Handle): ISR 中发送任务通知 ── */
static TaskHandle_t g_mpu_task_handle       = NULL;  /* MPU6050 姿态任务 */
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
    app_wheel_speed_target_t target;

    if (g_target_speed_queue == NULL) {
        return false;
    }

    target.left_rpm  = left_rpm;
    target.right_rpm = right_rpm;

    /* xQueueOverwrite: 队列满时覆盖旧值，保证不阻塞调用者 */
    return (xQueueOverwrite(g_target_speed_queue, &target) == pdPASS);
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
static void mpu_task(void *pvParameters)
{
    (void)pvParameters;
    attitude_msg_t msg = {0};

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
            msg.status  = 0;
            msg.pitch10 = pitch;   /* 俯仰角 (°) */
            msg.roll10  = roll;    /* 横滚角 (°) */
            msg.yaw10   = yaw;     /* 偏航角 (°) */
            /* 写入队列（预留给后续平衡/巡线任务消费） */
            xQueueOverwrite(g_attitude_queue, &msg);
        }
        /* 如果 Read_Quad() 失败，跳过本次，等待下一个中断 */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务 3: 速度档位切换任务 (优先级 2, 栈 192)
 *  ───────────────────────────────────────────
 *  功能: 检测用户按键 (PB21)，循环切换 8 个速度档位
 *  触发: 周期轮询 10ms，软件消抖集成在 key_read_user() 中
 *
 *  档位循环: 0→1→2→3→4→5→6→7 按下一次切换一档，8→0 回绕
 *  首次按下从停止状态进入档位 0 (20 RPM 前进)
 * ═══════════════════════════════════════════════════════════════════════════ */
static void speed_gear_task(void *pvParameters)
{
    (void)pvParameters;
    bool key_was = false;     /* 上一次按键状态 */
    int  gear_index = -1;     /* 当前档位索引, -1 = 停止 (初始状态) */

    for (;;) {
        /* 读取当前按键状态 (含软件消抖: 连续两次读到相同电平才确认) */
        bool key_now = key_read_user();

        /* 上升沿检测: 按键从未按下 → 按下 的跳变 */
        if (key_now && !key_was) {
            int32_t target;

            /* 档位递增 (首次按从 -1 → 0) */
            gear_index++;
            if (gear_index >= SPEED_GEAR_COUNT) {
                gear_index = 0;  /* 超出范围回绕到第一档 */
            }

            target = g_speed_gears[gear_index];

            /* 通过队列更新速度闭环任务的目标速度 (左右轮同步) */
            (void)app_tasks_set_wheel_speed_target(target, target);
        }

        key_was = key_now;  /* 保存当前状态用于下次边沿检测 */

        /* 10ms 轮询, 按键响应延迟 ≤ 20ms (2 次轮询) */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务 4: 速度闭环控制任务 (优先级 3 - 最高, 栈 384)
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
 * @brief 速度目标斜坡步进，每 50ms 最多变化 30RPM，降低换档/反向冲击
 */
#define SPEED_RAMP_STEP_RPM         30


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

    /* ── 初始化状态 ── */
    status.gear_index = -1;  /* 停止状态 */

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
            status.gear_index = -1;  /* 非档位模式 (如手动/巡线) */

            /* 尝试匹配档位: 如果左右目标一致且在档位表中, 记录档位索引 */
            if (new_target.left_rpm == new_target.right_rpm) {
                for (int i = 0; i < SPEED_GEAR_COUNT; i++) {
                    if (g_speed_gears[i] == new_target.left_rpm) {
                        status.gear_index = (int8_t)i;
                        break;
                    }
                }
            }

            /* 仅目标为 0 时立即停止并复位；普通换档/反向交给斜坡过渡，避免输出突变 */
            if (left_cmd_target_rpm == 0 && right_cmd_target_rpm == 0) {
                left_setpoint_rpm = 0;
                right_setpoint_rpm = 0;
                pid_inc_reset(&left_pid);
                pid_inc_reset(&right_pid);
                status.left_pwm  = 0;
                status.right_pwm = 0;
                tb6612_stop();
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
        status.seq++;                                        /* 消息序号递增 */
        status.t_ms       = (uint32_t)xTaskGetTickCount();   /* 系统滴答 (调试) */
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

            if (status.left_target_rpm == 0 && status.right_target_rpm == 0 &&
                status.left_setpoint_rpm == 0 && status.right_setpoint_rpm == 0) {
                /* 目标为零且斜坡已归零: 滑行停止 */
                tb6612_stop();
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
 *
 *  显示布局 (128×64 OLED):
 *  ┌────────────────────────────────────┐
 *  │ GEAR: 3  (档位 1~8 或 STOP)       │ 第 0 行 y=0
 *  │ T: 300/300    (目标速度 L/R RPM)    │ 第 1 行 y=16
 *  │ L: 298/45     (左轮实测/PWM)        │ 第 2 行 y=32
 *  │ R: 295/48     (右轮实测/PWM)        │ 第 3 行 y=48
 *  └────────────────────────────────────┘
 * ═══════════════════════════════════════════════════════════════════════════ */
static void oled_task(void *pvParameters)
{
    (void)pvParameters;
    speed_status_msg_t status = {0};  /* 本地缓存的显示状态 */
    uint16_t oled_clear_count = 0;  /* OLED 刷屏计数器 (调试用) */

    /* ── 初始化 OLED (SSD1306 软件 I2C, PA28=SDA, PA31=SCL) ── */
    OLED_Init();
    OLED_Clear();  /* 清屏 */

    for (;;) {
        speed_status_msg_t new_status;

        /* 非阻塞读取: 有新数据就更新本地缓存 */
        if (xQueueReceive(g_status_queue, &new_status, 0) == pdPASS) {
            status = new_status;
        }

        /* ── 刷新 OLED 显示 (16 号字体, 黑底白字) ── */
        oled_clear_count++;
        if (oled_clear_count >= 100) {
            /* 每 100 次刷新 (约 10s) 清屏一次, 避免残影 */
            OLED_Clear();
            oled_clear_count = 0;
        } 

        /* 第 0 行: 档位信息 */
        if (status.gear_index < 0) {
            /* 非档位模式 (停止/自由模式/外部设定) */
            OLED_ShowString(0, 0, "GEAR: STOP", 16, 1);
        } else {
            /* 显示档位号 (1-based, 用户友好) */
            OLED_vsprint(0, 0, 16, "GEAR:%d", (int)status.gear_index + 1);
        }

        /* 第 1 行: 目标速度 */
        OLED_vsprint(0, 16, 16, "T:%ld/%ld",
                     (long)status.left_target_rpm, (long)status.right_target_rpm);

        /* 第 2 行: 左轮实测速度 + PID 输出 */
        OLED_vsprint(0, 32, 16, "L:%ld/%ld",
                     (long)status.left_rpm, (long)status.left_pwm);

        /* 第 3 行: 右轮实测速度 + PID 输出 */
        OLED_vsprint(0, 48, 16, "R:%ld/%ld",
                     (long)status.right_rpm, (long)status.right_pwm);

        OLED_Refresh();  /* 显存 → 屏幕 */

        /* 100ms 刷新周期 (OLED I2C 传输耗约 30ms, 剩余时间让出 CPU) */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  任务 6: 调试打印任务 (优先级 1, 栈 256)
 *  ───────────────────────────────────────────
 *  功能: 从 status_queue 读取速度闭环状态, 每 50ms 输出 RPM 到串口
 *  用途: 串口波形查看 (SerialPlot / VOFA+ 等工具)
 *  格式: "左轮RPM,右轮RPM\r\n"
 * ═══════════════════════════════════════════════════════════════════════════ */
static void debug_print(void *pvParameters)
{
    (void)pvParameters;
    char buf[128];

    for (;;) {
        speed_status_msg_t new_status;

        if (xQueuePeek(g_status_queue, &new_status, 0) == pdPASS) {
            snprintf(buf, sizeof(buf), "%ld,%ld\r\n",
                     (long)new_status.left_rpm, (long)new_status.right_rpm);
            uart0_sendStr(buf);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
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

    /* 任何队列创建失败 → 不可恢复错误, 死循环 */
    if (g_attitude_queue == NULL || g_status_queue == NULL ||
        g_target_speed_queue == NULL) {
        while (1) {}
    }

    /* ── 创建任务 ──
     * xTaskCreate(任务函数, 任务名, 栈深度, 参数, 优先级, 任务句柄) */

    /* LED 心跳: 最低优先级, 最小栈 */
    xTaskCreate(led_task,        "LED",      128, NULL, 1, NULL);

    /* MPU 姿态: 需 I2C 通信栈 + DMP 浮点运算栈, 分配 512 */
    xTaskCreate(mpu_task,        "MPU",      512, NULL, 2, &g_mpu_task_handle);

    /* 速度闭环: 最高优先级, 含 PID 计算 , 分配 512 */
    xTaskCreate(speed_loop_task, "SPD_LOOP", 512, NULL, 3, &g_speed_loop_task_handle);

    /* 档位切换: 简单按键检测, 栈最小 */
    xTaskCreate(speed_gear_task, "GEAR",     192, NULL, 2, NULL);

    /* OLED 显示: 含 OLED 显存 (128×8=1024字节) + I2C 通信缓冲 */
    xTaskCreate(oled_task,       "OLED",     512, NULL, 1, NULL);

    /* DEBUG: 串口波形输出, snprintf + uart0_sendStr, 避开 printf semihosting */
    xTaskCreate(debug_print,     "DEBUG",    256, NULL, 1, NULL);

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
 * @note   每 10ms 产生一次中断, 发送任务通知唤醒 speed_loop_task
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
        /* 发送任务通知: 解除 speed_loop_task 的阻塞, 进入下一个控制周期 */
        if (g_speed_loop_task_handle != NULL) {
            vTaskNotifyGiveFromISR(g_speed_loop_task_handle,
                                   &xHigherPriorityTaskWoken);
        }
        break;
    default:
        break;
    }

    /* 请求上下文切换 (speed_loop_task 优先级 3 > 当前任务优先级时执行) */
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
