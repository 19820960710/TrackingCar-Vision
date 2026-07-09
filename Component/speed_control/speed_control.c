#include "speed_control/speed_control.h"
#include "encoder/encoder.h"
#include "tb6612/tb6612.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <stddef.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  速度环可调参数区
 * ═══════════════════════════════════════════════════════════════════════════ */
#define PID_DEFAULT_KP_MILLI             80
#define PID_DEFAULT_KI_MILLI             50
#define PID_DEFAULT_KD_MILLI             0
#define PID_OUTPUT_MIN                   (-80)
#define PID_OUTPUT_MAX                   80
#define ENCODER_SPEED_PERIOD_MS          10U
#define SPEED_RAMP_STEP_RPM              100
#define SPEED_START_FF_PWM               18   /* 过大会导致偶发末端小踢动 */
#define SPEED_START_FF_SETPOINT_RPM      90
#define SPEED_START_FF_ERR_RPM           2
#define SPEED_START_FF_MIN_SETPOINT_RPM  4
#define SPEED_START_FF_FULL_SETPOINT_RPM 24
#define SPEED_START_FF_MIN_PWM           12

typedef struct {
    bool wheels_stopped;
} speed_state_t;

static QueueHandle_t g_speed_target_queue = NULL;
static QueueHandle_t g_speed_state_queue = NULL;

static int32_t abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

bool speed_control_queue_init(void)
{
    speed_state_t initial_state = { true };

    g_speed_target_queue = xQueueCreate(1, sizeof(speed_target_msg_t));
    g_speed_state_queue = xQueueCreate(1, sizeof(speed_state_t));
    if (g_speed_target_queue == NULL || g_speed_state_queue == NULL) {
        return false;
    }
    (void)xQueueOverwrite(g_speed_state_queue, &initial_state);
    return true;
}

bool speed_control_set_target_with_ff(int32_t left_rpm,
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

bool speed_control_set_target(int32_t left_rpm, int32_t right_rpm)
{
    return speed_control_set_target_with_ff(left_rpm, right_rpm, false);
}

bool speed_control_receive_target(speed_target_msg_t *target)
{
    if (g_speed_target_queue == NULL || target == NULL) {
        return false;
    }
    return (xQueueReceive(g_speed_target_queue, target, 0) == pdPASS);
}

void speed_control_publish_state(const speed_loop_context_t *ctx)
{
    if (g_speed_state_queue == NULL) {
        return;
    }
    speed_state_t state = { speed_loop_is_wheels_stopped(ctx) };
    (void)xQueueOverwrite(g_speed_state_queue, &state);
}

bool speed_control_wheels_stopped_snapshot(void)
{
    speed_state_t state = {0};
    if (g_speed_state_queue == NULL ||
        xQueuePeek(g_speed_state_queue, &state, 0) != pdPASS) {
        return false;
    }
    return state.wheels_stopped;
}

void speed_control_pid_init(pid_inc_t *left_pid, pid_inc_t *right_pid)
{
    pid_inc_init(left_pid, PID_DEFAULT_KP_MILLI, PID_DEFAULT_KI_MILLI,
                 PID_DEFAULT_KD_MILLI, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    pid_inc_init(right_pid, PID_DEFAULT_KP_MILLI, PID_DEFAULT_KI_MILLI,
                 PID_DEFAULT_KD_MILLI, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
}

int32_t speed_control_ramp_step(int32_t current, int32_t target)
{
    int32_t step = SPEED_RAMP_STEP_RPM;

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

int32_t speed_control_apply_start_feedforward(int32_t pwm,
                                              int32_t setpoint_rpm,
                                              int32_t measured_rpm,
                                              bool enable)
{
    int32_t ff_pwm = SPEED_START_FF_PWM;
    if (ff_pwm < 0) {
        ff_pwm = -ff_pwm;
    }

    int32_t abs_setpoint = abs_i32(setpoint_rpm);
    if (!enable || ff_pwm == 0 || setpoint_rpm == 0 ||
        abs_setpoint > SPEED_START_FF_SETPOINT_RPM ||
        abs_setpoint < SPEED_START_FF_MIN_SETPOINT_RPM ||
        abs_i32(setpoint_rpm - measured_rpm) <= SPEED_START_FF_ERR_RPM) {
        return pwm;
    }

    if (abs_setpoint < SPEED_START_FF_FULL_SETPOINT_RPM) {
        int32_t ff_span = ff_pwm - SPEED_START_FF_MIN_PWM;
        if (ff_span > 0) {
            ff_pwm = SPEED_START_FF_MIN_PWM +
                (ff_span * abs_setpoint) / SPEED_START_FF_FULL_SETPOINT_RPM;
        }
    }

    int32_t sign = (setpoint_rpm > 0) ? 1 : -1;
    if (pwm != 0 && ((pwm > 0 && sign < 0) || (pwm < 0 && sign > 0))) {
        return pwm;
    }
    if (abs_i32(pwm) >= ff_pwm) {
        return pwm;
    }
    return (sign > 0) ? ff_pwm : -ff_pwm;
}

void speed_loop_init(speed_loop_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->left_rpm10_filt = 0;
    ctx->right_rpm10_filt = 0;
    ctx->left_target_rpm = 0;
    ctx->right_target_rpm = 0;
    ctx->left_setpoint_rpm = 0;
    ctx->right_setpoint_rpm = 0;
    ctx->left_delta_sum = 0;
    ctx->right_delta_sum = 0;
    ctx->sample_count = 0;
    ctx->left_rpm = 0;
    ctx->right_rpm = 0;
    ctx->left_pwm = 0;
    ctx->right_pwm = 0;

    speed_control_pid_init(&ctx->left_pid, &ctx->right_pid);
    encoder_reset();
}

void speed_loop_set_target(speed_loop_context_t *ctx, int32_t left_rpm, int32_t right_rpm)
{
    if (ctx == NULL) {
        return;
    }

    ctx->left_target_rpm = left_rpm;
    ctx->right_target_rpm = right_rpm;

    if (left_rpm == 0 && right_rpm == 0) {
        ctx->left_setpoint_rpm = 0;
        ctx->right_setpoint_rpm = 0;
        ctx->left_pwm = 0;
        ctx->right_pwm = 0;
        pid_inc_reset(&ctx->left_pid);
        pid_inc_reset(&ctx->right_pid);
        tb6612_brake();
    }
}

void speed_loop_update(speed_loop_context_t *ctx, bool start_ff_enable)
{
    if (ctx == NULL) {
        return;
    }

    encoder_data_t encoder;
    encoder_get_data(&encoder);
    ctx->left_delta_sum += encoder.left_delta;
    ctx->right_delta_sum += encoder.right_delta;
    ctx->sample_count++;

    if (ctx->sample_count < (SPEED_CONTROL_PERIOD_MS / ENCODER_SPEED_PERIOD_MS)) {
        return;
    }

    int32_t left_rpm10 = encoder_delta_to_rpm10(
        ctx->left_delta_sum, ctx->sample_count * ENCODER_SPEED_PERIOD_MS);
    int32_t right_rpm10 = encoder_delta_to_rpm10(
        ctx->right_delta_sum, ctx->sample_count * ENCODER_SPEED_PERIOD_MS);

    ctx->left_rpm10_filt += (left_rpm10 - ctx->left_rpm10_filt) / 2;
    ctx->right_rpm10_filt += (right_rpm10 - ctx->right_rpm10_filt) / 2;
    ctx->left_delta_sum = 0;
    ctx->right_delta_sum = 0;
    ctx->sample_count = 0;

    ctx->left_setpoint_rpm = speed_control_ramp_step(ctx->left_setpoint_rpm,
                                                     ctx->left_target_rpm);
    ctx->right_setpoint_rpm = speed_control_ramp_step(ctx->right_setpoint_rpm,
                                                      ctx->right_target_rpm);
    ctx->left_rpm = ctx->left_rpm10_filt / 10;
    ctx->right_rpm = ctx->right_rpm10_filt / 10;

    ctx->left_pwm = pid_inc_compute(&ctx->left_pid,
                                    ctx->left_setpoint_rpm,
                                    ctx->left_rpm);
    ctx->right_pwm = pid_inc_compute(&ctx->right_pid,
                                     ctx->right_setpoint_rpm,
                                     ctx->right_rpm);

    ctx->left_pwm = speed_control_apply_start_feedforward(ctx->left_pwm,
        ctx->left_setpoint_rpm, ctx->left_rpm, start_ff_enable);
    ctx->right_pwm = speed_control_apply_start_feedforward(ctx->right_pwm,
        ctx->right_setpoint_rpm, ctx->right_rpm, start_ff_enable);

    if (ctx->left_target_rpm == 0 && ctx->right_target_rpm == 0 &&
        ctx->left_setpoint_rpm == 0 && ctx->right_setpoint_rpm == 0) {
        tb6612_brake();
        ctx->left_pwm = 0;
        ctx->right_pwm = 0;
        pid_inc_reset(&ctx->left_pid);
        pid_inc_reset(&ctx->right_pid);
    } else {
        tb6612_set_speed((int16_t)ctx->right_pwm, (int16_t)ctx->left_pwm);
    }
}

bool speed_loop_is_wheels_stopped(const speed_loop_context_t *ctx)
{
    if (ctx == NULL) {
        return true;
    }
    return (ctx->left_target_rpm == 0 && ctx->right_target_rpm == 0 &&
            ctx->left_setpoint_rpm == 0 && ctx->right_setpoint_rpm == 0 &&
            ctx->left_pwm == 0 && ctx->right_pwm == 0);
}
