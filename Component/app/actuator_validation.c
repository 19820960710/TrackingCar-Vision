/**
 * @file actuator_validation.c
 * @brief 一次性执行两阶段执行机构组合验证。
 */
#include "app/actuator_validation.h"

#include "FreeRTOS.h"
#include "task.h"
#include "config/actuator_validation_config.h"
#include "task/app_tasks.h"
#include "zdt_x42s/zdt_x42s.h"

static volatile actuator_validation_stage_t g_stage =
    ACTUATOR_VALIDATION_WAITING;

volatile actuator_validation_debug_snapshot_t g_actuator_validation_debug;

static const app_stepper_move_t g_yaw_move = {
    .direction = APP_STEPPER_DIRECTION_CW,
    .speed_rpm = ACTUATOR_VALIDATION_STEPPER_SPEED_RPM,
    .acceleration = ACTUATOR_VALIDATION_STEPPER_ACCELERATION,
    .pulse_count = ACTUATOR_VALIDATION_STEPPER_PULSES,
    .motion_mode = ZDT_X42S_MOTION_RELATIVE_CURRENT,
    .sync_flag = 0U,
};

static const app_stepper_move_t g_pitch_move = {
    .direction = APP_STEPPER_DIRECTION_CW,
    .speed_rpm = ACTUATOR_VALIDATION_STEPPER_SPEED_RPM,
    .acceleration = ACTUATOR_VALIDATION_STEPPER_ACCELERATION,
    .pulse_count = ACTUATOR_VALIDATION_STEPPER_PULSES,
    .motion_mode = ZDT_X42S_MOTION_RELATIVE_CURRENT,
    .sync_flag = 0U,
};

actuator_validation_stage_t actuator_validation_get_stage(void)
{
    return g_stage;
}

static void update_debug_snapshot(void)
{
    app_stepper_state_t yaw_state = {0};
    app_stepper_state_t pitch_state = {0};

    (void)app_tasks_get_stepper_axis_state(APP_STEPPER_AXIS_YAW, &yaw_state);
    (void)app_tasks_get_stepper_axis_state(APP_STEPPER_AXIS_PITCH,
                                           &pitch_state);

    g_actuator_validation_debug.sequence++;
    g_actuator_validation_debug.stage = (uint32_t)g_stage;
    g_actuator_validation_debug.yaw_enabled = yaw_state.enabled ? 1U : 0U;
    g_actuator_validation_debug.pitch_enabled = pitch_state.enabled ? 1U : 0U;
    g_actuator_validation_debug.yaw_transmitted_commands =
        yaw_state.transmitted_commands;
    g_actuator_validation_debug.pitch_transmitted_commands =
        pitch_state.transmitted_commands;
    g_actuator_validation_debug.yaw_last_tx_ok = yaw_state.last_tx_ok ? 1U : 0U;
    g_actuator_validation_debug.pitch_last_tx_ok =
        pitch_state.last_tx_ok ? 1U : 0U;
    g_actuator_validation_debug.yaw_last_response = yaw_state.last_response;
    g_actuator_validation_debug.pitch_last_response = pitch_state.last_response;
    g_actuator_validation_debug.sequence++;
}

static void stop_all(void)
{
    (void)app_tasks_set_wheel_speed_target_mm_s(0.0f, 0.0f);
    (void)app_tasks_set_stepper_axis_enabled(APP_STEPPER_AXIS_YAW, false);
    (void)app_tasks_set_stepper_axis_enabled(APP_STEPPER_AXIS_PITCH, false);
}

static bool enable_axes(bool yaw, bool pitch)
{
    bool ok = true;

    if (yaw) {
        ok = app_tasks_set_stepper_axis_enabled(APP_STEPPER_AXIS_YAW, true) && ok;
    }
    if (pitch) {
        ok = app_tasks_set_stepper_axis_enabled(APP_STEPPER_AXIS_PITCH, true) && ok;
    }
    return ok;
}

static bool start_left_and_yaw(void)
{
    bool wheel_ok = app_tasks_set_wheel_speed_target_mm_s(
        ACTUATOR_VALIDATION_WHEEL_SPEED_MM_S, 0.0f);
    bool yaw_ok = app_tasks_move_stepper_axis(APP_STEPPER_AXIS_YAW,
                                               &g_yaw_move);
    return wheel_ok && yaw_ok;
}

static bool start_all_actuators(void)
{
    bool wheel_ok = app_tasks_set_wheel_speed_target_mm_s(
        ACTUATOR_VALIDATION_WHEEL_SPEED_MM_S,
        ACTUATOR_VALIDATION_WHEEL_SPEED_MM_S);
    bool yaw_ok = app_tasks_move_stepper_axis(APP_STEPPER_AXIS_YAW,
                                               &g_yaw_move);
    bool pitch_ok = app_tasks_move_stepper_axis(APP_STEPPER_AXIS_PITCH,
                                                 &g_pitch_move);
    return wheel_ok && yaw_ok && pitch_ok;
}

static void wait_for_stage_run_time(void)
{
    TickType_t start_tick = xTaskGetTickCount();
    const TickType_t run_ticks =
        pdMS_TO_TICKS(ACTUATOR_VALIDATION_STAGE_RUN_MS);

    while ((xTaskGetTickCount() - start_tick) < run_ticks) {
        update_debug_snapshot();
        vTaskDelay(pdMS_TO_TICKS(ACTUATOR_VALIDATION_POLL_PERIOD_MS));
    }
    update_debug_snapshot();
}

void actuator_validation_task(void *argument)
{
    (void)argument;

    update_debug_snapshot();
    vTaskDelay(pdMS_TO_TICKS(ACTUATOR_VALIDATION_START_DELAY_MS));

    if (!enable_axes(true, false)) {
        g_stage = ACTUATOR_VALIDATION_ERROR;
        update_debug_snapshot();
        stop_all();
        vTaskSuspend(NULL);
    }
    vTaskDelay(pdMS_TO_TICKS(ACTUATOR_VALIDATION_ENABLE_DELAY_MS));

    g_stage = ACTUATOR_VALIDATION_STAGE_LEFT_AND_YAW;
    update_debug_snapshot();
    if (!start_left_and_yaw()) {
        g_stage = ACTUATOR_VALIDATION_ERROR;
        update_debug_snapshot();
        stop_all();
        vTaskSuspend(NULL);
    }
    wait_for_stage_run_time();

    g_stage = ACTUATOR_VALIDATION_INTER_STAGE_STOP;
    update_debug_snapshot();
    (void)app_tasks_set_wheel_speed_target_mm_s(0.0f, 0.0f);
    vTaskDelay(pdMS_TO_TICKS(ACTUATOR_VALIDATION_INTER_STAGE_DELAY_MS));

    if (!enable_axes(false, true)) {
        g_stage = ACTUATOR_VALIDATION_ERROR;
        update_debug_snapshot();
        stop_all();
        vTaskSuspend(NULL);
    }
    vTaskDelay(pdMS_TO_TICKS(ACTUATOR_VALIDATION_ENABLE_DELAY_MS));

    g_stage = ACTUATOR_VALIDATION_STAGE_ALL;
    update_debug_snapshot();
    if (!start_all_actuators()) {
        g_stage = ACTUATOR_VALIDATION_ERROR;
        update_debug_snapshot();
        stop_all();
        vTaskSuspend(NULL);
    }
    wait_for_stage_run_time();

    g_stage = ACTUATOR_VALIDATION_STAGE_ALL_HOLD;
    update_debug_snapshot();
    vTaskDelay(pdMS_TO_TICKS(ACTUATOR_VALIDATION_FINAL_HOLD_MS));

    stop_all();
    vTaskDelay(pdMS_TO_TICKS(ACTUATOR_VALIDATION_POLL_PERIOD_MS));
    g_stage = ACTUATOR_VALIDATION_FINISHED;
    update_debug_snapshot();
    vTaskSuspend(NULL);
}
