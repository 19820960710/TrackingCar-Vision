/* ============================================================================
 *   闲鱼定制 小研分享屋
 *   任何非闲鱼小研分享屋出售的均为盗版
 *   正式比赛代码绑定机器绑定芯片，任何二手出售均无效
 *   请认准正版
 * ============================================================================ */

/**
 * @file    attitude_service.c
 * @brief   姿态 service 层实现：MPU 稳定检测、yaw 零点归一与姿态快照发布。
 *
 * @details 本模块不直接访问 ICM20602 驱动；attitude_task 读取 Mahony 解算后的
 *          pitch/roll/yaw 传入，经稳定检测后通过队列发布。
 *
 *          === 稳定检测机制 ===
 *          复位后需等待三轴在 80 个采样窗口（~20s@1kHz）内波动不超过阈值：
 *          - pitch/roll: ±1.5°
 *          - yaw: ±1.0°
 *          稳定时锁存 yaw 零点偏移，此后所有姿态的 yaw 减去该偏移并归一化，
 *          实现上电自动归零（不需要手动校准）。
 *
 *          === 队列模型 ===
 *          姿态通过长度为 1 的队列（覆盖写）发布，外部用 xQueuePeek 非破坏性读取。
 *          仅保留最新姿态，消费线程不会移走数据。
 *
 *          === yaw 解卷绕 ===
 *          稳定检测窗口内 yaw 可能跨 ±180° 边界，使用 unwrap 机制避免上下界失效。
 */
#include "service/attitude_service.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <stdint.h>

/* 稳定检测窗口参数 */
#define MPU_STABLE_REQUIRED_SAMPLES    80U    /* 稳定窗口采样数（@1kHz 约 80ms，实际因 I2C 周期约 20s） */
#define MPU_STABLE_PITCH_RANGE_DEG     1.50f  /* pitch 窗口内最大允许波动 (°) */
#define MPU_STABLE_ROLL_RANGE_DEG      1.50f  /* roll 窗口内最大允许波动 (°) */
#define MPU_STABLE_YAW_RANGE_DEG       1.00f  /* yaw 窗口内最大允许波动 (°) */

/**
 * @brief  稳定检测器状态机。
 *
 *         累计 N 个采样帧，统计三轴的 min/max 范围。
 *         窗口满时如果三轴波动均 < 阈值，锁存 yaw 零点并置 ready=true。
 *         如果窗口满但波动超阈值，重置窗口重新统计。
 */
typedef struct {
    uint16_t stable_count;       /* 窗口内已累计采样数 */
    float pitch_min;             /* 窗口内 pitch 最小值 */
    float pitch_max;             /* 窗口内 pitch 最大值 */
    float roll_min;              /* 窗口内 roll 最小值 */
    float roll_max;              /* 窗口内 roll 最大值 */
    float yaw_min;               /* 窗口内 yaw 最小值（解卷绕后） */
    float yaw_max;               /* 窗口内 yaw 最大值（解卷绕后） */
    float yaw_window_ref;        /* yaw 解卷绕参考点，避免跨 ±180° 跳变 */
    float yaw_zero_offset;       /* 稳定时锁存的 yaw 零点偏移 */
    bool ready;                  /* true=已稳定，后续采样直接发布 */
} attitude_stabilizer_t;

static QueueHandle_t g_attitude_queue = NULL;  /* 姿态队列单例（长度 1，覆盖写） */
static attitude_stabilizer_t g_stabilizer;     /* 稳定检测器单例 */


/**
 * @brief  浮点角度（度）转 deg10 整数（四舍五入）。
 */
static int32_t attitude_float_to_deg10(float angle_deg)
{
    if (angle_deg >= 0.0f) {
        return (int32_t)(angle_deg * 10.0f + 0.5f);
    }
    return (int32_t)(angle_deg * 10.0f - 0.5f);
}

/**
 * @brief  浮点角度归一化到 (-180, 180]。
 */
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

/**
 * @brief  组装 app_attitude_t 消息并覆盖写入姿态队列。
 *         队列长度为 1，写操作总是立即成功（不阻塞）。
 *
 * @param  pitch_deg  pitch 角度（度）
 * @param  roll_deg   roll 角度（度）
 * @param  yaw_deg    yaw 角度（度，已减去零点偏移）
 * @param  valid      true=姿态有效，false=MPU 失效（用于错误通知下游）
 */
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
    /* 全零初始化结构体：清零累计计数、上下界、ready 标志 */
    attitude_stabilizer_t init = {0};
    g_stabilizer = init;
}


/**
 * @brief  喂入一帧采样到稳定检测器，判定姿态是否已稳定。
 *
 * @param  pitch_deg  pitch 角度（度）
 * @param  roll_deg   roll 角度（度）
 * @param  yaw_deg    yaw 角度（度）
 * @return true=已稳定（ready），false=仍需累计或窗口超阈值需重置
 *
 * @note   yaw 解卷绕逻辑：窗口内采样以 yaw_window_ref 为参考点，
 *         每次新采样计算到参考点的最短角差 δ，用 ref+δ 作为解卷绕值。
 *         这样即使 yaw 跨过 ±180° 边界，上下界统计也不会失效。
 */
static bool attitude_stabilizer_accept(float pitch_deg,
                                       float roll_deg,
                                       float yaw_deg)
{
    attitude_stabilizer_t *st = &g_stabilizer;

    if (st->ready) {
        return true;   /* 已稳定，后续采样直接放行 */
    }

    /* 窗口首个采样：初始化上下界与参考点 */
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

    /* yaw 解卷绕：以参考点为基准取最短角差，避免跨 ±180° 时上下界失效 */
    float yaw_unwrapped = st->yaw_window_ref +
        attitude_normalize_deg(yaw_deg - st->yaw_window_ref);

    /* 更新窗口内三轴上下界 */
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

    /* 窗口满：检查三轴波动是否均在阈值内 */
    if (((st->pitch_max - st->pitch_min) <= MPU_STABLE_PITCH_RANGE_DEG) &&
        ((st->roll_max - st->roll_min) <= MPU_STABLE_ROLL_RANGE_DEG) &&
        ((st->yaw_max - st->yaw_min) <= MPU_STABLE_YAW_RANGE_DEG)) {
        /* 达标：锁存 yaw 零点偏移（当前采样角度），置 ready */
        st->yaw_zero_offset = yaw_deg;
        st->ready = true;
        return true;
    }

    /* 不达标：重置窗口重新统计（从头再来） */
    attitude_service_reset();
    return false;
}


bool attitude_service_init(void)
{
    if (g_attitude_queue == NULL) {
        /* 创建长度为 1 的队列，每个元素为 app_attitude_t 结构体 */
        g_attitude_queue = xQueueCreate(1, sizeof(app_attitude_t));
    }
    attitude_service_reset();
    return (g_attitude_queue != NULL);
}


void attitude_service_publish_invalid(void)
{
    /* 发布一条 valid=false 的姿态，下游（yaw 环/OLED）据此知道 MPU 不可用 */
    attitude_publish(0.0f, 0.0f, 0.0f, false);
}


void attitude_service_process_sample(float pitch_deg,
                                     float roll_deg,
                                     float yaw_deg)
{
    /* 未稳定时不发布，等稳定窗口达标后再开始输出。
     * 这样可以避免上电初期姿态漂移被 yaw 环误用。 */
    if (!attitude_stabilizer_accept(pitch_deg, roll_deg, yaw_deg)) {
        return;
    }

    /* 稳定后：yaw 减去稳定时锁存的零点偏移并归一化到 (-180, 180]，
     * 实现上电自动归零——小车放置方向即为 0°。 */
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
