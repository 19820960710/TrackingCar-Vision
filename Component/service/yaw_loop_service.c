/* ============================================================================
 *   闲鱼定制 小研分享屋
 *   任何非闲鱼小研分享屋出售的均为盗版
 *   正式比赛代码绑定机器绑定芯片，任何二手出售均无效
 *   请认准正版
 * ============================================================================ */

/**
 * @file    yaw_loop_service.c
 * @brief   yaw 角闭环 service 层实现：目标/状态队列管理、10ms→50ms 分频与串级控制逻辑。
 *
 * @details 每 10ms 由 yaw_loop_task（TIMER_0 中断通知）唤醒，内部 5 分频，
 *          每 50ms 执行一次 yaw_control_update()，将 base_speed/turn_rpm/前馈/到位状态
 *          写入状态快照，供 app_tasks 胶水层读取后做差速换算和速度环下发。
 *
 *          架构设计：本模块只做 yaw 环计算，不再直接调用 speed_service。
 *          差速换算与速度环下发交由 app_tasks 胶水层完成，使得 yaw 与 speed
 *          两个环可以在调试时独立使能/屏蔽，降低耦合。
 *
 *          队列模型：目标/状态队列各长度 1（覆盖写），外部通过 setter/getter 访问，
 *          不暴露队列句柄，保持封装性。
 */
#include "service/yaw_loop_service.h"
#include "service/attitude_service.h"
#include "control/yaw_control.h"
#include "FreeRTOS.h"
#include "queue.h"

/**
 * @brief  yaw 目标消息结构体（模块内部使用）。
 */
typedef struct {
    app_yaw_target_t value;      /* 目标值：基准速度 + 目标角度 */
    bool enabled;                /* true=使能 yaw 闭环 */
    bool reset_pid;              /* true=通知 step 入口复位 PID（用于目标切换时清积分） */
} yaw_target_msg_t;

/**
 * @brief  yaw 环运行时上下文（模块内部单例）。
 */
typedef struct {
    yaw_control_t control;        /* yaw 纯算法控制器 */
    yaw_target_msg_t target;      /* 当前生效的目标消息 */
    app_yaw_status_t status;      /* 最新状态快照（供 OLED/等待接口读取） */
    uint32_t tick_divider;        /* 10ms 分频计数器，满 5 次（50ms）执行一次 yaw 控制 */
} yaw_loop_context_t;

#define YAW_LOOP_TIMER_TICK_MS      10   /* TIMER_0 硬件节拍 10ms */
#define YAW_LOOP_PERIOD_MS          50   /* yaw 控制周期 50ms（与 speed 环对齐） */
#define YAW_LOOP_DIVIDER            (YAW_LOOP_PERIOD_MS / YAW_LOOP_TIMER_TICK_MS)  /* = 5 */

static QueueHandle_t g_yaw_target_queue = NULL;  /* 目标队列（长度 1，覆盖写） */
static QueueHandle_t g_yaw_state_queue = NULL;   /* 状态队列（长度 1，覆盖写） */
static yaw_loop_context_t g_yaw_ctx;             /* yaw 环单例上下文 */


/**
 * @brief  50ms yaw 控制主体：读姿态 → 算 PID → 写状态快照。
 *
 *         根据使能标志和姿态有效性，三种路径：
 *         1. 未使能：复位 PID，turn_rpm=0，不干预速度环（交上层）；
 *         2. 使能但姿态不可用：复位 PID，turn_rpm=0，attitude_valid=false
 *            上层据此安全停车；
 *         3. 使能且姿态有效：正常闭环，输出 base/turn/前馈标志。
 *
 * @param  ctx  yaw 环上下文指针
 */
static void yaw_loop_update_50ms(yaw_loop_context_t *ctx)
{
    app_attitude_t attitude;
    app_yaw_status_t *status = &ctx->status;
    const yaw_target_msg_t *target = &ctx->target;

    /* 先把目标同步到状态快照，保证下游可见（OLED/等待接口） */
    status->base_speed_rpm = target->value.base_speed_rpm;
    status->target_yaw_deg10 = target->value.target_yaw_deg10;
    status->enabled = target->enabled;
    status->settled = false;
    status->speed_ff_enable = false;
    status->attitude_valid = false;

    if (!target->enabled) {
        /* 路径 1：yaw 环未使能 —— 复位控制器，不输出转向 */
        yaw_control_reset(&ctx->control);
        status->turn_rpm = 0;
    } else if (!attitude_service_get(&attitude) || !attitude.valid) {
        /* 路径 2：yaw 使能但姿态不可用（MPU 未稳定或通信丢失）
         * 复位控制器，attitude_valid=false 告知上层安全停车 */
        yaw_control_reset(&ctx->control);
        status->turn_rpm = 0;
    } else {
        /* 路径 3：正常闭环 —— 计算 yaw PID */
        yaw_control_output_t control_out;

        /* 记录当前 yaw 角和误差（归一化到 ±180°） */
        status->current_yaw_deg10 = yaw_normalize_deg10(attitude.yaw_deg10);
        status->error_yaw_deg10 = yaw_normalize_deg10(
            target->value.target_yaw_deg10 - status->current_yaw_deg10);

        /* 执行 yaw 控制算法：内部包含目标斜坡、PID、动态限幅、到位锁存等 */
        yaw_control_update(&ctx->control,
                           target->value.target_yaw_deg10,
                           status->current_yaw_deg10,
                           &control_out);

        /* 将算法输出同步到状态快照 */
        status->turn_rpm = control_out.turn_rpm;
        status->settled = control_out.settled;
        status->speed_ff_enable = control_out.speed_ff_enable;
        status->attitude_valid = true;
    }

    /* 覆盖写入队列，下游通过 peek 非破坏性读取 */
    (void)xQueueOverwrite(g_yaw_state_queue, status);
}


bool yaw_loop_service_init(void)
{
    /* 创建目标/状态队列，长度 1，覆盖写模式 */
    if (g_yaw_target_queue == NULL) {
        g_yaw_target_queue = xQueueCreate(1, sizeof(yaw_target_msg_t));
    }
    if (g_yaw_state_queue == NULL) {
        g_yaw_state_queue = xQueueCreate(1, sizeof(app_yaw_status_t));
    }
    if (g_yaw_target_queue == NULL || g_yaw_state_queue == NULL) {
        return false;
    }

    /* 写入初始状态：目标未使能，但标记 reset_pid=true 确保首次 step 复位控制器 */
    yaw_target_msg_t initial_target = {0};
    app_yaw_status_t initial_status = {0};
    initial_target.reset_pid = true;
    (void)xQueueOverwrite(g_yaw_target_queue, &initial_target);
    (void)xQueueOverwrite(g_yaw_state_queue, &initial_status);

    /* 清零上下文并初始化 yaw 控制器（装入默认 PID 参数） */
    yaw_loop_context_t init = {0};
    g_yaw_ctx = init;
    yaw_control_init(&g_yaw_ctx.control);
    return true;
}


bool yaw_loop_service_step_10ms(void)
{
    yaw_loop_context_t *ctx = &g_yaw_ctx;

    /* ────── 1. 取最新 yaw 目标 ────── */
    /* 如果队列有新目标，取出并检查是否需要复位 PID（切换目标时清除积分残值） */
    if (xQueueReceive(g_yaw_target_queue, &ctx->target, 0) == pdPASS &&
        ctx->target.reset_pid) {
        yaw_control_reset(&ctx->control);
    }

    /* ────── 2. 10ms 分频：每 5 次才执行一次 yaw 控制 ────── */
    ctx->tick_divider++;
    if (ctx->tick_divider < YAW_LOOP_DIVIDER) {
        return false;   /* 未到 50ms，yaw 控制不执行 */
    }
    ctx->tick_divider = 0;

    /* ────── 3. 执行 50ms yaw 控制 ────── */
    yaw_loop_update_50ms(ctx);
    return true;        /* 告知调用方：本周期完成了 yaw PID 计算 */
}


bool yaw_loop_service_set_target(int32_t base_speed_rpm,
                                 int32_t target_yaw_deg10)
{
    yaw_target_msg_t target = {0};
    app_yaw_status_t state = {0};

    if (g_yaw_target_queue == NULL || g_yaw_state_queue == NULL) {
        return false;
    }

    /* 构造目标消息：目标归一到 ±180°，使能 yaw 环，要求复位 PID */
    target.value.base_speed_rpm = base_speed_rpm;
    target.value.target_yaw_deg10 = yaw_normalize_deg10(target_yaw_deg10);
    target.enabled = true;
    target.reset_pid = true;

    /* 立即更新状态快照，让 OLED/等待接口无需等下个 50ms 周期就能看到新目标 */
    (void)xQueuePeek(g_yaw_state_queue, &state, 0);
    state.base_speed_rpm = target.value.base_speed_rpm;
    state.target_yaw_deg10 = target.value.target_yaw_deg10;
    state.enabled = target.enabled;
    state.settled = false;
    (void)xQueueOverwrite(g_yaw_state_queue, &state);

    /* 将目标消息写入队列（覆盖写） */
    return (xQueueOverwrite(g_yaw_target_queue, &target) == pdPASS);
}


bool yaw_loop_service_get_status(app_yaw_status_t *out)
{
    if (out == NULL || g_yaw_state_queue == NULL ||
        xQueuePeek(g_yaw_state_queue, out, 0) != pdPASS) {
        return false;
    }
    return true;
}


bool yaw_loop_service_is_settled(void)
{
    app_yaw_status_t status = {0};

    if (!yaw_loop_service_get_status(&status)) {
        return false;
    }
    /* 已使能且进入到位/保持区才算 settled。
     * 未使能时 settled=false 可防止误判。 */
    return status.enabled && status.settled;
}
