/* ============================================================================
 *   闲鱼定制 小研分享屋
 *   任何非闲鱼小研分享屋出售的均为盗版
 *   正式比赛代码绑定机器绑定芯片，任何二手出售均无效
 *   请认准正版
 * ============================================================================ */

/**
 * @file speed_loop_core.c
 * @brief MG680 双轮 30 ms、mm/s 位置式 PI 速度环实现。
 */
#include "control/speed_loop_core.h"

#include <stddef.h>

static float abs_f32(float value)
{
    return (value < 0.0f) ? -value : value;
}

static int32_t abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static float clamp_f32(float value, float min_value, float max_value)
{
    if (value > max_value) {
        return max_value;
    }
    if (value < min_value) {
        return min_value;
    }
    return value;
}

static int8_t direction_from_target(float target_mm_s)
{
    if (target_mm_s > 0.0f) {
        return 1;
    }
    if (target_mm_s < 0.0f) {
        return -1;
    }
    return 0;
}

static void reset_motor_state(speed_loop_motor_state_t *state)
{
    speed_loop_motor_state_t reset = {0};
    *state = reset;
}

void speed_loop_core_init(speed_loop_core_t *ctx,
                          const speed_loop_config_t *config)
{
    uint32_t side;

    if ((ctx == NULL) || (config == NULL)) {
        return;
    }
    ctx->config = *config;
    ctx->sample_count = 0U;
    for (side = 0U; side < (uint32_t)SPEED_LOOP_SIDE_COUNT; ++side) {
        reset_motor_state(&ctx->motor[side]);
    }
}

static void set_motor_target(speed_loop_motor_state_t *state, float target_mm_s)
{
    int8_t requested_direction = direction_from_target(target_mm_s);

    state->target_mm_s = target_mm_s;
    state->requested_direction = requested_direction;
    if (requested_direction == 0) {
        state->integral_mm = 0.0f;
        state->last_error_mm_s = 0.0f;
        state->error_mm_s = 0.0f;
        state->pwm_duty_count = 0;
        state->active_direction = 0;
        state->direction_change_pending = false;
    } else if (state->active_direction == 0) {
        state->active_direction = requested_direction;
        state->last_error_mm_s = abs_f32(target_mm_s) -
                                 state->measured_speed_mm_s;
    } else if (state->active_direction != requested_direction) {
        state->integral_mm = 0.0f;
        state->last_error_mm_s = 0.0f;
        state->pwm_duty_count = 0;
        state->direction_change_pending = true;
    }
}

void speed_loop_core_set_target_mm_s(speed_loop_core_t *ctx,
                                     float left_mm_s,
                                     float right_mm_s)
{
    if (ctx == NULL) {
        return;
    }
    set_motor_target(&ctx->motor[SPEED_LOOP_SIDE_LEFT], left_mm_s);
    set_motor_target(&ctx->motor[SPEED_LOOP_SIDE_RIGHT], right_mm_s);
}

static void update_motor(speed_loop_core_t *ctx, speed_loop_side_t side)
{
    speed_loop_motor_state_t *state = &ctx->motor[side];
    const speed_loop_motor_config_t *pid = &ctx->config.motor[side];
    const float dt_s = (float)ctx->config.control_period_ms / 1000.0f;
    float raw_speed = 0.0f;
    float alpha = clamp_f32(ctx->config.filter_alpha, 0.0f, 1.0f);

    if ((pid->counts_per_wheel_rev > 0.0f) &&
        (ctx->config.wheel_circumference_mm > 0.0f)) {
        raw_speed = ((float)abs_i32(state->encoder_delta_sum) *
                     ctx->config.wheel_circumference_mm /
                     pid->counts_per_wheel_rev) / dt_s;
    }
    state->encoder_delta_sum = 0;
    state->raw_speed_mm_s = raw_speed;
    state->measured_speed_mm_s =
        state->measured_speed_mm_s * (1.0f - alpha) + raw_speed * alpha;

    if (state->requested_direction == 0) {
        state->pwm_duty_count = 0;
        return;
    }

    if (state->direction_change_pending) {
        if (state->measured_speed_mm_s > ctx->config.direction_change_stop_mm_s) {
            state->pwm_duty_count = 0;
            return;
        }
        state->active_direction = state->requested_direction;
        state->direction_change_pending = false;
        state->integral_mm = 0.0f;
        state->last_error_mm_s = 0.0f;
    }

    {
        float integral_limit = abs_f32(pid->integral_limit);
        float candidate_integral;
        float derivative;
        float duty;

        state->error_mm_s = abs_f32(state->target_mm_s) -
                            state->measured_speed_mm_s;
        candidate_integral = clamp_f32(
            state->integral_mm + state->error_mm_s * dt_s,
            -integral_limit, integral_limit);
        derivative = (state->error_mm_s - state->last_error_mm_s) / dt_s;
        duty = pid->kp * state->error_mm_s +
               pid->ki * candidate_integral +
               pid->kd * derivative;

        if (((duty < (float)ctx->config.pwm_logical_max) ||
             (state->error_mm_s < 0.0f)) &&
            ((duty > 0.0f) || (state->error_mm_s > 0.0f))) {
            state->integral_mm = candidate_integral;
        }
        state->last_error_mm_s = state->error_mm_s;
        duty = clamp_f32(duty, 0.0f, (float)ctx->config.pwm_logical_max);
        state->pwm_duty_count = (int32_t)duty * state->active_direction;
    }
}

bool speed_loop_core_update_sample(speed_loop_core_t *ctx,
                                   int32_t left_delta,
                                   int32_t right_delta,
                                   speed_loop_core_output_t *out)
{
    uint32_t samples_per_control;

    if ((ctx == NULL) || (ctx->config.sample_period_ms == 0U) ||
        (ctx->config.control_period_ms == 0U)) {
        return false;
    }
    samples_per_control = ctx->config.control_period_ms /
                          ctx->config.sample_period_ms;
    if (samples_per_control == 0U) {
        return false;
    }
    ctx->motor[SPEED_LOOP_SIDE_LEFT].encoder_delta_sum += left_delta;
    ctx->motor[SPEED_LOOP_SIDE_RIGHT].encoder_delta_sum += right_delta;
    ctx->sample_count++;
    if (ctx->sample_count < samples_per_control) {
        speed_loop_core_get_output(ctx, out);
        return false;
    }
    ctx->sample_count = 0U;
    update_motor(ctx, SPEED_LOOP_SIDE_LEFT);
    update_motor(ctx, SPEED_LOOP_SIDE_RIGHT);
    speed_loop_core_get_output(ctx, out);
    return true;
}

void speed_loop_core_get_output(const speed_loop_core_t *ctx,
                                speed_loop_core_output_t *out)
{
    if ((ctx == NULL) || (out == NULL)) {
        return;
    }
    out->left_speed_mm_s = ctx->motor[SPEED_LOOP_SIDE_LEFT].measured_speed_mm_s;
    out->right_speed_mm_s = ctx->motor[SPEED_LOOP_SIDE_RIGHT].measured_speed_mm_s;
    out->left_target_mm_s = ctx->motor[SPEED_LOOP_SIDE_LEFT].target_mm_s;
    out->right_target_mm_s = ctx->motor[SPEED_LOOP_SIDE_RIGHT].target_mm_s;
    out->left_pwm_duty_count = ctx->motor[SPEED_LOOP_SIDE_LEFT].pwm_duty_count;
    out->right_pwm_duty_count = ctx->motor[SPEED_LOOP_SIDE_RIGHT].pwm_duty_count;
    out->stopped = speed_loop_core_is_stopped(ctx);
    out->brake = out->stopped;
}

bool speed_loop_core_is_stopped(const speed_loop_core_t *ctx)
{
    if (ctx == NULL) {
        return true;
    }
    return (ctx->motor[SPEED_LOOP_SIDE_LEFT].target_mm_s == 0.0f) &&
           (ctx->motor[SPEED_LOOP_SIDE_RIGHT].target_mm_s == 0.0f) &&
           (ctx->motor[SPEED_LOOP_SIDE_LEFT].pwm_duty_count == 0) &&
           (ctx->motor[SPEED_LOOP_SIDE_RIGHT].pwm_duty_count == 0);
}
