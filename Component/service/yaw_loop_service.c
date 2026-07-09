#include "service/yaw_loop_service.h"
#include "service/attitude_service.h"
#include "control/yaw_control.h"
#include "control/speed_control.h"
#include "FreeRTOS.h"
#include "queue.h"

typedef struct {
    app_yaw_target_t value;
    bool enabled;
    bool reset_pid;
} yaw_target_msg_t;

typedef struct {
    yaw_control_t control;
    yaw_target_msg_t target;
    app_yaw_status_t status;
    uint32_t tick_divider;
} yaw_loop_context_t;

#define YAW_LOOP_TIMER_TICK_MS      10
#define YAW_LOOP_PERIOD_MS          50
#define YAW_LOOP_DIVIDER            (YAW_LOOP_PERIOD_MS / YAW_LOOP_TIMER_TICK_MS)

static QueueHandle_t g_yaw_target_queue = NULL;
static QueueHandle_t g_yaw_state_queue = NULL;
static yaw_loop_context_t g_yaw_ctx;

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
    app_attitude_t attitude;
    app_yaw_status_t *status = &ctx->status;
    const yaw_target_msg_t *target = &ctx->target;

    status->base_speed_rpm = target->value.base_speed_rpm;
    status->target_yaw_deg10 = target->value.target_yaw_deg10;
    status->enabled = target->enabled;
    status->settled = false;

    if (!target->enabled) {
        yaw_control_reset(&ctx->control);
        status->turn_rpm = 0;
    } else if (!attitude_service_get(&attitude) || !attitude.valid) {
        yaw_control_reset(&ctx->control);
        status->turn_rpm = 0;
        (void)speed_control_set_target(0, 0);
    } else {
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

        int32_t left_cmd_rpm = target->value.base_speed_rpm - status->turn_rpm;
        int32_t right_cmd_rpm = target->value.base_speed_rpm + status->turn_rpm;
        (void)speed_control_set_target_with_ff(left_cmd_rpm,
                                               right_cmd_rpm,
                                               control_out.speed_ff_enable);
    }

    (void)xQueueOverwrite(g_yaw_state_queue, status);
}

bool yaw_loop_service_init(void)
{
    if (g_yaw_target_queue == NULL) {
        g_yaw_target_queue = xQueueCreate(1, sizeof(yaw_target_msg_t));
    }
    if (g_yaw_state_queue == NULL) {
        g_yaw_state_queue = xQueueCreate(1, sizeof(app_yaw_status_t));
    }
    if (g_yaw_target_queue == NULL || g_yaw_state_queue == NULL) {
        return false;
    }

    yaw_target_msg_t initial_target = {0};
    app_yaw_status_t initial_status = {0};
    initial_target.reset_pid = true;
    (void)xQueueOverwrite(g_yaw_target_queue, &initial_target);
    (void)xQueueOverwrite(g_yaw_state_queue, &initial_status);
    yaw_loop_init_context(&g_yaw_ctx);
    return true;
}

void yaw_loop_service_step_10ms(void)
{
    yaw_loop_receive_target(&g_yaw_ctx);
    if (yaw_loop_period_elapsed(&g_yaw_ctx)) {
        yaw_loop_update_50ms(&g_yaw_ctx);
    }
}

bool yaw_loop_service_set_target(int32_t base_speed_rpm,
                                 int32_t target_yaw_deg10)
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
    return status.enabled && status.settled;
}
