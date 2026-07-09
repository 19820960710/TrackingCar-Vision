/**
 * @file    attitude_service.c
 * @brief   姿态 service 层实现：MPU6050 稳定检测、yaw 零点归一与姿态快照发布。
 *
 * @details 本模块不直接访问 MPU6050 驱动；attitude_task 读取 DMP 后将
 *          pitch/roll/yaw 传入。复位后需等待三轴在稳定窗口（80 个采样）内
 *          满足阈值才发布有效姿态（实测约 20s）。姿态通过长度 1 的队列发布，
 *          外部 peek 只读取最新值。稳定前 yaw 慢漂，稳定时锁存零点偏移并归一。
 */
#include "service/attitude_service.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <stdint.h>

#define MPU_STABLE_REQUIRED_SAMPLES    80U    /* 稳定窗口采样数 */
#define MPU_STABLE_PITCH_RANGE_DEG     1.50f  /* pitch 窗口内最大波动阈值 */
#define MPU_STABLE_ROLL_RANGE_DEG      1.50f  /* roll 窗口内最大波动阈值 */
#define MPU_STABLE_YAW_RANGE_DEG       1.00f  /* yaw 窗口内最大波动阈值 */

/** 稳定检测器：累计窗口内三轴上下界，达标后锁存 yaw 零点并置 ready。 */
typedef struct {
    uint16_t stable_count;       /* 窗口内已累计采样数 */
    float pitch_min;             /* 窗口内 pitch 下界 */
    float pitch_max;             /* 窗口内 pitch 上界 */
    float roll_min;              /* 窗口内 roll 下界 */
    float roll_max;              /* 窗口内 roll 上界 */
    float yaw_min;               /* 窗口内 yaw 下界（解卷绕后） */
    float yaw_max;               /* 窗口内 yaw 上界（解卷绕后） */
    float yaw_window_ref;        /* yaw 解卷绕参考点，避免 ±180° 跳变 */
    float yaw_zero_offset;       /* 稳定时锁存的 yaw 零点偏移 */
    bool ready;                  /* true=已稳定，后续采样直接发布 */
} attitude_stabilizer_t;

static QueueHandle_t g_attitude_queue = NULL;  /* 姿态队列（长度 1，覆盖写） */
static attitude_stabilizer_t g_stabilizer;     /* 稳定检测器单例 */

/* 浮点角度转 deg10（四舍五入），正负分别处理避免截断偏差。 */
static int32_t attitude_float_to_deg10(float angle_deg)
{
    if (angle_deg >= 0.0f) {
        return (int32_t)(angle_deg * 10.0f + 0.5f);
    }
    return (int32_t)(angle_deg * 10.0f - 0.5f);
}

/* 浮点角度归一化到 (-180, 180]。 */
static float attitude_normalize_deg(float angle)
{
    while (angle > 180.0f) {
        angle -= 360.0f;
    }
    while (angle <= -180.0f) {
        angle += 360.0f;
    }
    return angle;
}

/* 组装 app_attitude_t 并覆盖写入姿态队列。 */
static void attitude_publish(float pitch_deg,
                             float roll_deg,
                             float yaw_deg,
                             bool valid)
{
    if (g_attitude_queue == NULL) {
        return;
    }

    app_attitude_t msg = {0};
    msg.pitch_deg10 = attitude_float_to_deg10(pitch_deg);
    msg.roll_deg10 = attitude_float_to_deg10(roll_deg);
    msg.yaw_deg10 = attitude_float_to_deg10(yaw_deg);
    msg.valid = valid;
    (void)xQueueOverwrite(g_attitude_queue, &msg);
}

void attitude_service_reset(void)
{
    attitude_stabilizer_t init = {0};
    g_stabilizer = init;
}

/* 喂入一帧采样并判定是否稳定。未达采样数或超阈值返回 false（需继续累计）；
 * 达标且三轴波动均在阈值内则锁存 yaw 零点并返回 true；不达标则重置窗口。 */
static bool attitude_stabilizer_accept(float pitch_deg,
                                       float roll_deg,
                                       float yaw_deg)
{
    attitude_stabilizer_t *st = &g_stabilizer;

    if (st->ready) {
        return true;   /* 已稳定，后续采样直接放行 */
    }

    /* 窗口首个采样：初始化上下界。 */
    if (st->stable_count == 0U) {
        st->pitch_min = pitch_deg;
        st->pitch_max = pitch_deg;
        st->roll_min = roll_deg;
        st->roll_max = roll_deg;
        st->yaw_window_ref = yaw_deg;
        st->yaw_min = yaw_deg;
        st->yaw_max = yaw_deg;
        st->stable_count = 1U;
        return false;
    }

    /* yaw 解卷绕：以参考点为基准取最短角差，避免跨 ±180° 时上下界失效。 */
    float yaw_unwrapped = st->yaw_window_ref +
        attitude_normalize_deg(yaw_deg - st->yaw_window_ref);

    /* 更新窗口内三轴上下界。 */
    if (pitch_deg < st->pitch_min) st->pitch_min = pitch_deg;
    if (pitch_deg > st->pitch_max) st->pitch_max = pitch_deg;
    if (roll_deg < st->roll_min) st->roll_min = roll_deg;
    if (roll_deg > st->roll_max) st->roll_max = roll_deg;
    if (yaw_unwrapped < st->yaw_min) st->yaw_min = yaw_unwrapped;
    if (yaw_unwrapped > st->yaw_max) st->yaw_max = yaw_unwrapped;

    st->stable_count++;
    if (st->stable_count < MPU_STABLE_REQUIRED_SAMPLES) {
        return false;   /* 采样数不足，继续累计 */
    }

    /* 窗口满：三轴波动均在阈值内则锁存零点并置 ready。 */
    if (((st->pitch_max - st->pitch_min) <= MPU_STABLE_PITCH_RANGE_DEG) &&
        ((st->roll_max - st->roll_min) <= MPU_STABLE_ROLL_RANGE_DEG) &&
        ((st->yaw_max - st->yaw_min) <= MPU_STABLE_YAW_RANGE_DEG)) {
        st->yaw_zero_offset = yaw_deg;
        st->ready = true;
        return true;
    }

    /* 不达标：重置窗口重新累计。 */
    attitude_service_reset();
    return false;
}

bool attitude_service_init(void)
{
    if (g_attitude_queue == NULL) {
        g_attitude_queue = xQueueCreate(1, sizeof(app_attitude_t));
    }
    attitude_service_reset();
    return (g_attitude_queue != NULL);
}

void attitude_service_publish_invalid(void)
{
    attitude_publish(0.0f, 0.0f, 0.0f, false);
}

void attitude_service_process_sample(float pitch_deg,
                                     float roll_deg,
                                     float yaw_deg)
{
    /* 未稳定时不发布，等稳定窗口达标。 */
    if (!attitude_stabilizer_accept(pitch_deg, roll_deg, yaw_deg)) {
        return;
    }

    /* 稳定后：yaw 减去锁存的零点偏移并归一，发布有效姿态。 */
    attitude_publish(pitch_deg,
                     roll_deg,
                     attitude_normalize_deg(yaw_deg - g_stabilizer.yaw_zero_offset),
                     true);
}

bool attitude_service_get(app_attitude_t *out)
{
    if (out == NULL || g_attitude_queue == NULL ||
        xQueuePeek(g_attitude_queue, out, 0) != pdPASS) {
        return false;
    }
    return true;
}
