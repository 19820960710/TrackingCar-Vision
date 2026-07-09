#include "control/speed_loop_core.h"
#include <stddef.h>

#define PID_DEFAULT_KP_MILLI             80
#define PID_DEFAULT_KI_MILLI             50
#define PID_DEFAULT_KD_MILLI             0
#define PID_OUTPUT_MIN                   (-80)
#define PID_OUTPUT_MAX                   80
#define SPEED_RAMP_STEP_RPM              100
#define SPEED_START_FF_PWM               18
#define SPEED_START_FF_SETPOINT_RPM      90
#define SPEED_START_FF_ERR_RPM           2
#define SPEED_START_FF_MIN_SETPOINT_RPM  4
#define SPEED_START_FF_FULL_SETPOINT_RPM 24
#define SPEED_START_FF_MIN_PWM           12

static int32_t abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int32_t delta_to_rpm10(int32_t delta,
                              uint32_t period_ms,
                              uint32_t counts_per_rev)
{
    if (period_ms == 0U || counts_per_rev == 0U) {
        return 0;
    }
    return (int32_t)(((int64_t)delta * 600000) /
                     ((int64_t)counts_per_rev * period_ms));
}

static void pid_init(pid_inc_t *left_pid, pid_inc_t *right_pid)
{
    pid_inc_init(left_pid, PID_DEFAULT_KP_MILLI, PID_DEFAULT_KI_MILLI,
                 PID_DEFAULT_KD_MILLI, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    pid_inc_init(right_pid, PID_DEFAULT_KP_MILLI, PID_DEFAULT_KI_MILLI,
                 PID_DEFAULT_KD_MILLI, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
}

static int32_t ramp_step(int32_t current, int32_t target)
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

static int32_t apply_start_feedforward(int32_t pwm,
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

void speed_loop_core_init(speed_loop_core_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    speed_loop_core_t init = {0};
    *ctx = init;
    pid_init(&ctx->left_pid, &ctx->right_pid);
}

bool speed_loop_core_set_target(speed_loop_core_t *ctx,
                                int32_t left_rpm,
                                int32_t right_rpm)
{
    if (ctx == NULL) {
        return false;
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
        return true;
    }

    return false;
}

void speed_loop_core_get_output(const speed_loop_core_t *ctx,
                                speed_loop_core_output_t *out)
{
    if (ctx == NULL || out == NULL) {
        return;
    }

    out->left_rpm = ctx->left_rpm;
    out->right_rpm = ctx->right_rpm;
    out->left_target_rpm = ctx->left_target_rpm;
    out->right_target_rpm = ctx->right_target_rpm;
    out->left_pwm = ctx->left_pwm;
    out->right_pwm = ctx->right_pwm;
    out->stopped = speed_loop_core_is_stopped(ctx);
    out->brake = out->stopped;
}

bool speed_loop_core_update(speed_loop_core_t *ctx,
                            int32_t left_delta,
                            int32_t right_delta,
                            uint32_t sample_period_ms,
                            uint32_t counts_per_rev,
                            bool start_ff_enable,
                            speed_loop_core_output_t *out)
{
    if (ctx == NULL) {
        return false;
    }

    ctx->left_delta_sum += left_delta;
    ctx->right_delta_sum += right_delta;
    ctx->sample_count++;

    if (ctx->sample_count <
        (SPEED_LOOP_CORE_CONTROL_PERIOD_MS / SPEED_LOOP_CORE_SAMPLE_PERIOD_MS)) {
        if (out != NULL) {
            speed_loop_core_get_output(ctx, out);
        }
        return false;
    }

    uint32_t period_ms = ctx->sample_count * sample_period_ms;
    int32_t left_rpm10 = delta_to_rpm10(ctx->left_delta_sum,
                                        period_ms,
                                        counts_per_rev);
    int32_t right_rpm10 = delta_to_rpm10(ctx->right_delta_sum,
                                         period_ms,
                                         counts_per_rev);

    ctx->left_rpm10_filt += (left_rpm10 - ctx->left_rpm10_filt) / 2;
    ctx->right_rpm10_filt += (right_rpm10 - ctx->right_rpm10_filt) / 2;
    ctx->left_delta_sum = 0;
    ctx->right_delta_sum = 0;
    ctx->sample_count = 0;

    ctx->left_setpoint_rpm = ramp_step(ctx->left_setpoint_rpm,
                                       ctx->left_target_rpm);
    ctx->right_setpoint_rpm = ramp_step(ctx->right_setpoint_rpm,
                                        ctx->right_target_rpm);
    ctx->left_rpm = ctx->left_rpm10_filt / 10;
    ctx->right_rpm = ctx->right_rpm10_filt / 10;

    ctx->left_pwm = pid_inc_compute(&ctx->left_pid,
                                    ctx->left_setpoint_rpm,
                                    ctx->left_rpm);
    ctx->right_pwm = pid_inc_compute(&ctx->right_pid,
                                     ctx->right_setpoint_rpm,
                                     ctx->right_rpm);

    ctx->left_pwm = apply_start_feedforward(ctx->left_pwm,
        ctx->left_setpoint_rpm, ctx->left_rpm, start_ff_enable);
    ctx->right_pwm = apply_start_feedforward(ctx->right_pwm,
        ctx->right_setpoint_rpm, ctx->right_rpm, start_ff_enable);

    if (ctx->left_target_rpm == 0 && ctx->right_target_rpm == 0 &&
        ctx->left_setpoint_rpm == 0 && ctx->right_setpoint_rpm == 0) {
        ctx->left_pwm = 0;
        ctx->right_pwm = 0;
        pid_inc_reset(&ctx->left_pid);
        pid_inc_reset(&ctx->right_pid);
    }

    if (out != NULL) {
        speed_loop_core_get_output(ctx, out);
    }
    return true;
}

bool speed_loop_core_is_stopped(const speed_loop_core_t *ctx)
{
    if (ctx == NULL) {
        return true;
    }
    return (ctx->left_target_rpm == 0 && ctx->right_target_rpm == 0 &&
            ctx->left_setpoint_rpm == 0 && ctx->right_setpoint_rpm == 0 &&
            ctx->left_pwm == 0 && ctx->right_pwm == 0);
}
