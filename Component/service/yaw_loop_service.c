/**
 * @file    yaw_loop_service.c
 * @brief   yaw 角闭环 service 层实现：目标/状态队列与串级 step 逻辑。
 *
 * @details 每 10ms 由 yaw_loop_task 唤醒，内部 5 分频，每 50ms 执行一次
 *          yaw_control_update()，输出 turn_rpm 经差速换算后下发到 speed_service。
 *          yaw 先更新目标并下发轮速，再由任务层通知速度环执行，保证时序。
 *          目标/状态队列模块私有，外部通过 setter/getter 访问。
 */
#include "service/yaw_loop_service.h"
#include "service/attitude_service.h"
#include "control/yaw_control.h"
#include "service/speed_service.h"
#include "FreeRTOS.h"
#include "queue.h"

/** yaw 目标消息：目标值 + 使能标志 + 是否需要重置 PID。 */
typedef struct {
    app_yaw_target_t value;
    bool enabled;
    bool reset_pid;
} yaw_target_msg_t;

/** yaw 环运行时上下文：控制器 + 当前目标 + 状态快照 + 10ms 分频计数。 */
typedef struct {
    yaw_control_t control;        /* yaw 纯算法控制器 */
    yaw_target_msg_t target;      /* 当前生效的目标消息 */
    app_yaw_status_t status;      /* 最新状态快照 */
    uint32_t tick_divider;        /* 10ms 计数，满 5 次（50ms）执行一次控制 */
} yaw_loop_context_t;

#define YAW_LOOP_TIMER_TICK_MS      10   /* TIMER_0 节拍 10ms */
#define YAW_LOOP_PERIOD_MS          50   /* yaw 控制周期 50ms */
#define YAW_LOOP_DIVIDER            (YAW_LOOP_PERIOD_MS / YAW_LOOP_TIMER_TICK_MS)

static QueueHandle_t g_yaw_target_queue = NULL;  /* 目标队列（长度 1，覆盖写） */
static QueueHandle_t g_yaw_state_queue = NULL;   /* 状态队列（长度 1，覆盖写） */
static yaw_loop_context_t g_yaw_ctx;             /* yaw 环单例上下文 */

/* 50ms yaw 控制主体：读姿态 -> 算 PID -> 差速下发 -> 发布状态。 */
static void yaw_loop_update_50ms(yaw_loop_context_t *ctx)
{
    app_attitude_t attitude;
    app_yaw_status_t *status = &ctx->status;
    const yaw_target_msg_t *target = &ctx->target;

    /* 先把目标同步到状态快照，保证下游可见。 */
    status->base_speed_rpm = target->value.base_speed_rpm;
    status->target_yaw_deg10 = target->value.target_yaw_deg10;
    status->enabled = target->enabled;
    status->settled = false;

    if (!target->enabled) {
        /* 未使能：复位控制器，不输出转向。 */
        yaw_control_reset(&ctx->control);
        status->turn_rpm = 0;
    } else if (!attitude_service_get(&attitude) || !attitude.valid) {
        /* 姿态不可用（MPU 未稳定/失效）：复位控制器并停车保安全。 */
        yaw_control_reset(&ctx->control);
        status->turn_rpm = 0;
        (void)speed_service_set_target(0, 0);
    } else {
        /* 正常闭环：计算 yaw PID 并差速下发。 */
        yaw_control_output_t control_out;

        status->current_yaw_deg10 = yaw_normalize_deg10(attitude.yaw_deg10);
        status->error_yaw_deg10 = yaw_normalize_deg10(
            target->value.target_yaw_deg10 - status->current_yaw_deg10);

        yaw_control_update(&ctx->control,
                           target->value.target_yaw_deg10,
                           status->current_yaw_deg10,
                           &control_out);

        status->turn_rpm = control_out.turn_rpm;
        status->settled = control_out.settled;

        /* 差速换算：左轮减、右轮加 turn_rpm；实测方向正确。 */
        int32_t left_cmd_rpm = target->value.base_speed_rpm - status->turn_rpm;
        int32_t right_cmd_rpm = target->value.base_speed_rpm + status->turn_rpm;
        (void)speed_service_set_target_with_ff(left_cmd_rpm,
                                               right_cmd_rpm,
                                               control_out.speed_ff_enable);
    }

    (void)xQueueOverwrite(g_yaw_state_queue, status);
}

bool yaw_loop_service_init(void)
{
    /* 创建目标/状态队列（长度 1，覆盖写）。 */
    if (g_yaw_target_queue == NULL) {
        g_yaw_target_queue = xQueueCreate(1, sizeof(yaw_target_msg_t));
    }
    if (g_yaw_state_queue == NULL) {
        g_yaw_state_queue = xQueueCreate(1, sizeof(app_yaw_status_t));
    }
    if (g_yaw_target_queue == NULL || g_yaw_state_queue == NULL) {
        return false;
    }

    /* 写入初始状态：目标未使能但要求一次 PID reset，状态全零。 */
    yaw_target_msg_t initial_target = {0};
    app_yaw_status_t initial_status = {0};
    initial_target.reset_pid = true;
    (void)xQueueOverwrite(g_yaw_target_queue, &initial_target);
    (void)xQueueOverwrite(g_yaw_state_queue, &initial_status);

    /* 清零上下文并初始化控制器。 */
    yaw_loop_context_t init = {0};
    g_yaw_ctx = init;
    yaw_control_init(&g_yaw_ctx.control);
    return true;
}

void yaw_loop_service_step_10ms(void)
{
    yaw_loop_context_t *ctx = &g_yaw_ctx;

    /* 取最新 yaw 目标，带 reset_pid 标志时重置控制器。 */
    if (xQueueReceive(g_yaw_target_queue, &ctx->target, 0) == pdPASS &&
        ctx->target.reset_pid) {
        yaw_control_reset(&ctx->control);
    }

    /* 10ms 节拍，每 5 次（50ms）才执行一次 yaw 控制。 */
    ctx->tick_divider++;
    if (ctx->tick_divider < YAW_LOOP_DIVIDER) {
        return;
    }
    ctx->tick_divider = 0;

    yaw_loop_update_50ms(ctx);
}

bool yaw_loop_service_set_target(int32_t base_speed_rpm,
                                 int32_t target_yaw_deg10)
{
    yaw_target_msg_t target = {0};
    app_yaw_status_t state = {0};

    if (g_yaw_target_queue == NULL || g_yaw_state_queue == NULL) {
        return false;
    }

    /* 目标归一到 ±180°，标记使能并要求 PID reset。 */
    target.value.base_speed_rpm = base_speed_rpm;
    target.value.target_yaw_deg10 = yaw_normalize_deg10(target_yaw_deg10);
    target.enabled = true;
    target.reset_pid = true;

    /* 立即更新状态快照，让 OLED/等待接口无需等下一个 50ms 周期即可看到新目标。 */
    (void)xQueuePeek(g_yaw_state_queue, &state, 0);
    state.base_speed_rpm = target.value.base_speed_rpm;
    state.target_yaw_deg10 = target.value.target_yaw_deg10;
    state.enabled = target.enabled;
    state.settled = false;
    (void)xQueueOverwrite(g_yaw_state_queue, &state);

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
    /* 已使能且进入到位/保持区才算 settled。 */
    return status.enabled && status.settled;
}
