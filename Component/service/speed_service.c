/* ============================================================================
 *   闲鱼定制 小研分享屋
 *   任何非闲鱼小研分享屋出售的均为盗版
 *   正式比赛代码绑定机器绑定芯片，任何二手出售均无效
 *   请认准正版
 * ============================================================================ */

/**
 * @file speed_service.c
 * @brief 速度核心与 FreeRTOS 队列、编码器、TB6612 之间的适配。
 */
#include "service/speed_service.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "config/motor_speed_profiles.h"
#include "control/speed_loop_core.h"
#include "encoder/encoder.h"
#include "tb6612/tb6612.h"

#include <stddef.h>

typedef struct {
    float left_mm_s;
    float right_mm_s;
} speed_target_msg_t;

static QueueHandle_t g_speed_target_queue = NULL;
static QueueHandle_t g_speed_state_queue = NULL;
static speed_loop_core_t g_speed_core;

static speed_loop_config_t g_active_speed_config;

static void build_control_config(const motor_speed_profile_t *profile,
                                 speed_loop_config_t *config)
{
    speed_loop_config_t reset = {0};
    *config = reset;

    config->motor[SPEED_LOOP_SIDE_LEFT].kp =
        profile->pid[MOTOR_PROFILE_SIDE_LEFT].kp;
    config->motor[SPEED_LOOP_SIDE_LEFT].ki =
        profile->pid[MOTOR_PROFILE_SIDE_LEFT].ki;
    config->motor[SPEED_LOOP_SIDE_LEFT].kd =
        profile->pid[MOTOR_PROFILE_SIDE_LEFT].kd;
    config->motor[SPEED_LOOP_SIDE_LEFT].integral_limit =
        profile->pid[MOTOR_PROFILE_SIDE_LEFT].integral_limit;
    config->motor[SPEED_LOOP_SIDE_LEFT].counts_per_wheel_rev =
        profile->output_counts_per_wheel_rev[MOTOR_PROFILE_SIDE_LEFT];

    config->motor[SPEED_LOOP_SIDE_RIGHT].kp =
        profile->pid[MOTOR_PROFILE_SIDE_RIGHT].kp;
    config->motor[SPEED_LOOP_SIDE_RIGHT].ki =
        profile->pid[MOTOR_PROFILE_SIDE_RIGHT].ki;
    config->motor[SPEED_LOOP_SIDE_RIGHT].kd =
        profile->pid[MOTOR_PROFILE_SIDE_RIGHT].kd;
    config->motor[SPEED_LOOP_SIDE_RIGHT].integral_limit =
        profile->pid[MOTOR_PROFILE_SIDE_RIGHT].integral_limit;
    config->motor[SPEED_LOOP_SIDE_RIGHT].counts_per_wheel_rev =
        profile->output_counts_per_wheel_rev[MOTOR_PROFILE_SIDE_RIGHT];

    config->wheel_circumference_mm =
        motor_speed_profile_wheel_circumference_mm(profile);
    config->filter_alpha = profile->speed_filter_alpha;
    config->direction_change_stop_mm_s =
        profile->direction_change_stop_mm_s;
    config->sample_period_ms = profile->sample_period_ms;
    config->control_period_ms = profile->control_period_ms;
    config->pwm_logical_max = profile->pwm_logical_max;
}

static void publish_snapshot(void)
{
    speed_loop_core_output_t output = {0};
    speed_service_state_t state = {0};

    speed_loop_core_get_output(&g_speed_core, &output);
    state.left_speed_mm_s = output.left_speed_mm_s;
    state.right_speed_mm_s = output.right_speed_mm_s;
    state.left_target_mm_s = output.left_target_mm_s;
    state.right_target_mm_s = output.right_target_mm_s;
    state.left_pwm_duty_count = output.left_pwm_duty_count;
    state.right_pwm_duty_count = output.right_pwm_duty_count;
    state.stopped = output.stopped;
    (void)xQueueOverwrite(g_speed_state_queue, &state);
}

bool speed_service_init(void)
{
    if (!motor_speed_profile_is_usable(MOTOR_SPEED_ACTIVE_PROFILE) ||
        (MOTOR_SPEED_ACTIVE_PROFILE->sample_period_ms !=
         SPEED_SERVICE_SAMPLE_PERIOD_MS)) {
        return false;
    }
    if (g_speed_target_queue == NULL) {
        g_speed_target_queue = xQueueCreate(1, sizeof(speed_target_msg_t));
    }
    if (g_speed_state_queue == NULL) {
        g_speed_state_queue = xQueueCreate(1, sizeof(speed_service_state_t));
    }
    if ((g_speed_target_queue == NULL) || (g_speed_state_queue == NULL)) {
        return false;
    }

    build_control_config(MOTOR_SPEED_ACTIVE_PROFILE, &g_active_speed_config);
    speed_loop_core_init(&g_speed_core, &g_active_speed_config);
    encoder_reset();
    publish_snapshot();
    return true;
}

bool speed_service_set_target_mm_s(float left_mm_s, float right_mm_s)
{
    speed_target_msg_t target = {left_mm_s, right_mm_s};

    if (g_speed_target_queue == NULL) {
        return false;
    }
    return xQueueOverwrite(g_speed_target_queue, &target) == pdPASS;
}

void speed_service_step_10ms(void)
{
    speed_target_msg_t new_target;
    encoder_data_t encoder;
    speed_loop_core_output_t output = {0};

    if ((g_speed_target_queue != NULL) &&
        (xQueueReceive(g_speed_target_queue, &new_target, 0) == pdPASS)) {
        speed_loop_core_set_target_mm_s(&g_speed_core,
                                        new_target.left_mm_s,
                                        new_target.right_mm_s);
        if ((new_target.left_mm_s == 0.0f) &&
            (new_target.right_mm_s == 0.0f)) {
            tb6612_brake();
        }
    }

    encoder_get_data(&encoder);
    if (speed_loop_core_update_sample(&g_speed_core,
                                      encoder.left_delta,
                                      encoder.right_delta,
                                      &output)) {
        if (output.brake) {
            tb6612_brake();
        } else {
            tb6612_set_duty_count(
                output.left_pwm_duty_count *
                    MOTOR_SPEED_ACTIVE_PROFILE->drive_direction_sign[
                        MOTOR_PROFILE_SIDE_LEFT],
                output.right_pwm_duty_count *
                    MOTOR_SPEED_ACTIVE_PROFILE->drive_direction_sign[
                        MOTOR_PROFILE_SIDE_RIGHT]);
        }
    }
    publish_snapshot();
}

bool speed_service_get_state(speed_service_state_t *out)
{
    if ((g_speed_state_queue == NULL) || (out == NULL)) {
        return false;
    }
    return xQueuePeek(g_speed_state_queue, out, 0) == pdPASS;
}

bool speed_service_wheels_stopped_snapshot(void)
{
    speed_service_state_t state = {0};
    return speed_service_get_state(&state) && state.stopped;
}
