/**
 * @file    speed_service.c
 * @brief   速度环适配层实现：FreeRTOS 队列 + 编码器读取 + TB6612 输出。
 *
 * @details 分层原则——纯算法在 speed_loop_core 中，本文件负责 RTOS 适配与硬件 I/O：
 *          - 管理速度目标/状态队列（模块私有句柄，外部通过 setter/getter 访问）；
 *          - 10ms 节拍入口读编码器增量，累计到 50ms 由算法核心做 PID；
 *          - 核心 update 返回 true 时输出 PWM 到 TB6612；
 *          - 目标为 0/0 时核心立即停止并返回制动请求。
 *
 *          数据流（每 10ms）：
 *          speed_service_step_10ms()
 *            ├─ 取队列最新目标 → speed_loop_core_set_target()
 *            ├─ 读编码器增量 → speed_loop_core_update()
 *            │    └─ 满 50ms 时：PID → PWM → tb6612_set_speed()
 *            └─ 发布速度状态快照到队列
 */
#include "service/speed_service.h"
#include "control/speed_loop_core.h"
#include "encoder/encoder.h"
#include "tb6612/tb6612.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <stddef.h>

/**
 * @brief  速度目标消息结构体（模块内部使用，不暴露给外部）。
 */
typedef struct {
    int32_t left_rpm;            /* 左轮目标 RPM */
    int32_t right_rpm;           /* 右轮目标 RPM */
    bool low_speed_ff_enable;    /* yaw 环请求启用低速前馈 */
} speed_target_msg_t;

static QueueHandle_t g_speed_target_queue = NULL;  /* 目标队列（长度 1，覆盖写） */
static QueueHandle_t g_speed_state_queue = NULL;   /* 状态队列（长度 1，覆盖写） */
static speed_loop_core_t g_speed_core;             /* 速度环纯算法核心单例 */
static bool g_low_speed_ff_enable = false;         /* 当前前馈使能状态，由 yaw 环设置 */


/**
 * @brief  从算法核心取当前输出并发布状态快照到队列。
 *         每次 step_10ms 末尾调用，确保下游（OLED/等待接口）能读到最新状态。
 */
static void speed_service_publish_snapshot(void)
{
    speed_loop_core_output_t output = {0};
    speed_loop_core_get_output(&g_speed_core, &output);

    speed_service_state_t state = {0};
    state.left_rpm = output.left_rpm;
    state.right_rpm = output.right_rpm;
    state.left_target_rpm = output.left_target_rpm;
    state.right_target_rpm = output.right_target_rpm;
    state.stopped = output.stopped;
    (void)xQueueOverwrite(g_speed_state_queue, &state);
}


bool speed_service_init(void)
{
    /* 创建目标/状态队列，长度 1，覆盖写模式（只保留最新值，不累积） */
    if (g_speed_target_queue == NULL) {
        g_speed_target_queue = xQueueCreate(1, sizeof(speed_target_msg_t));
    }
    if (g_speed_state_queue == NULL) {
        g_speed_state_queue = xQueueCreate(1, sizeof(speed_service_state_t));
    }
    if (g_speed_target_queue == NULL || g_speed_state_queue == NULL) {
        return false;
    }

    g_low_speed_ff_enable = false;
    speed_loop_core_init(&g_speed_core);
    encoder_reset();
    speed_service_publish_snapshot();   /* 发布初始"已停稳"快照 */
    return true;
}


bool speed_service_set_target_with_ff(int32_t left_rpm,
                                      int32_t right_rpm,
                                      bool low_speed_ff_enable)
{
    speed_target_msg_t target = {0};

    if (g_speed_target_queue == NULL) {
        return false;
    }
    target.left_rpm = left_rpm;
    target.right_rpm = right_rpm;
    target.low_speed_ff_enable = low_speed_ff_enable;
    return (xQueueOverwrite(g_speed_target_queue, &target) == pdPASS);
}


bool speed_service_set_target(int32_t left_rpm, int32_t right_rpm)
{
    /* 应用层公开接口：默认关闭 yaw 专用低速前馈 */
    return speed_service_set_target_with_ff(left_rpm, right_rpm, false);
}


void speed_service_step_10ms(void)
{
    /* ────── 1. 取最新速度目标 ────── */
    /* 如果队列有新的目标值，取出并喂给算法核心。
     * speed_loop_core_set_target() 在目标为 0/0 时立即停止并返回 true，
     * 此时需要调用 tb6612_brake() 做电机短接制动。 */
    speed_target_msg_t new_target;
    if (g_speed_target_queue != NULL &&
        xQueueReceive(g_speed_target_queue, &new_target, 0) == pdPASS) {
        g_low_speed_ff_enable = new_target.low_speed_ff_enable;
        if (speed_loop_core_set_target(&g_speed_core,
                                       new_target.left_rpm,
                                       new_target.right_rpm)) {
            /* 目标为 0/0：立即制动并发布状态，不再走下面的采样+计算 */
            tb6612_brake();
            speed_service_publish_snapshot();
        }
    }

    /* ────── 2. 10ms 编码器采样 ────── */
    /* 读取左右编码器增量（delta），喂给算法核心。
     * 核心内部累计到 50ms 才执行 PID，提前返回 false。 */
    encoder_data_t encoder;
    encoder_get_data(&encoder);

    speed_loop_core_output_t output = {0};
    if (speed_loop_core_update(&g_speed_core,
                               encoder.left_delta,
                               encoder.right_delta,
                               SPEED_LOOP_CORE_SAMPLE_PERIOD_MS,
                               ENCODER_COUNTS_PER_REV,
                               g_low_speed_ff_enable,
                               &output)) {
        /* ────── 3. 满 50ms 窗口：核心已完成 PID ────── */
        if (output.brake) {
            /* 核心要求制动（目标 0/0 且已完全停稳） */
            tb6612_brake();
        } else {
            /* 正常输出 PWM：
             * tb6612_set_speed(right_pwm, left_pwm) 参数顺序：
             * 第 1 个 = 右轮，第 2 个 = 左轮 */
            tb6612_set_speed((int16_t)output.right_pwm,
                             (int16_t)output.left_pwm);
        }
    }

    /* ────── 4. 发布最新状态快照（供下游 OLED/等待接口使用） ────── */
    speed_service_publish_snapshot();
}


bool speed_service_get_state(speed_service_state_t *out)
{
    if (g_speed_state_queue == NULL || out == NULL ||
        xQueuePeek(g_speed_state_queue, out, 0) != pdPASS) {
        return false;
    }
    return true;
}


bool speed_service_wheels_stopped_snapshot(void)
{
    speed_service_state_t state = {0};
    if (!speed_service_get_state(&state)) {
        return false;
    }
    return state.stopped;
}
