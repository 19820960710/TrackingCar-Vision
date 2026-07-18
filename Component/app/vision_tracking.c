/**
 * @file vision_tracking.c
 * @brief Target-centering outer loop for the yaw and pitch steppers.
 */
#include "app/vision_tracking.h"

#include "FreeRTOS.h"
#include "task.h"
#include "config/vision_tracking_config.h"
#include "control/gimbal_pd_tracker.h"
#include "service/stepper_service.h"
#include "vision/vision_uart.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

typedef struct {
    stepper_axis_t axis;
    uint16_t speed_rpm;
    uint8_t acceleration;
} vision_tracking_axis_t;

volatile vision_tracking_debug_t g_vision_tracking_debug = {0};

static bool submit_axis_move(const vision_tracking_axis_t *axis,
                             int32_t signed_pulses)
{
    stepper_motor_move_t move;

    move.direction = (signed_pulses > 0) ? ZDT_X42S_DIRECTION_CW :
                                           ZDT_X42S_DIRECTION_CCW;
    move.speed_rpm = axis->speed_rpm;
    move.acceleration = axis->acceleration;
    move.pulse_count = (uint32_t)((signed_pulses < 0) ?
                         -signed_pulses : signed_pulses);
    move.motion_mode = 0U;
    move.sync_flag = 0U;
    if (!stepper_service_move_axis(axis->axis, &move)) {
        return false;
    }
    return true;
}

static void send_telemetry(uint32_t now_ms)
{
    static uint32_t last_telemetry_ms;
    char line[80];
    int length;

    if ((uint32_t)(now_ms - last_telemetry_ms) <
        VISION_TRACKING_TELEMETRY_PERIOD_MS) {
        return;
    }
    length = snprintf(line, sizeof(line), "VT,%lu,%lu,%ld,%ld,%ld,%ld",
                      (unsigned long)now_ms,
                      (unsigned long)g_vision_tracking_debug.last_target_valid,
                      (long)g_vision_tracking_debug.last_error_x_pixels,
                      (long)g_vision_tracking_debug.last_error_y_pixels,
                      (long)g_vision_tracking_debug.yaw_output_pulses,
                      (long)g_vision_tracking_debug.pitch_output_pulses);
    if ((length > 0) && ((size_t)length < sizeof(line)) &&
        vision_uart_send_line(line)) {
        last_telemetry_ms = now_ms;
    }
}

void vision_tracking_task(void *argument)
{
    vision_tracking_axis_t yaw = {
        .axis = STEPPER_AXIS_YAW,
        .speed_rpm = VISION_TRACKING_SPEED_RPM,
        .acceleration = VISION_TRACKING_ACCELERATION,
    };
    vision_tracking_axis_t pitch = {
        .axis = STEPPER_AXIS_PITCH,
        .speed_rpm = VISION_TRACKING_SPEED_RPM,
        .acceleration = VISION_TRACKING_ACCELERATION,
    };
    const gimbal_pd_tracker_config_t yaw_config = {
        .kp_pulses_per_pixel = VISION_TRACKING_YAW_KP_PULSES_PER_PIXEL,
        .kd_pulse_seconds_per_pixel =
            VISION_TRACKING_YAW_KD_PULSE_SECONDS_PER_PIXEL,
        .deadband_pixels = VISION_TRACKING_DEADBAND_PIXELS,
        .maximum_pulses = VISION_TRACKING_YAW_MAX_PULSES,
        .command_period_ms = VISION_TRACKING_COMMAND_PERIOD_MS,
        .positive_error_is_cw = VISION_TRACKING_YAW_POSITIVE_IS_CW,
    };
    const gimbal_pd_tracker_config_t pitch_config = {
        .kp_pulses_per_pixel = VISION_TRACKING_PITCH_KP_PULSES_PER_PIXEL,
        .kd_pulse_seconds_per_pixel =
            VISION_TRACKING_PITCH_KD_PULSE_SECONDS_PER_PIXEL,
        .deadband_pixels = VISION_TRACKING_DEADBAND_PIXELS,
        .maximum_pulses = VISION_TRACKING_PITCH_MAX_PULSES,
        .command_period_ms = VISION_TRACKING_COMMAND_PERIOD_MS,
        .positive_error_is_cw = VISION_TRACKING_PITCH_POSITIVE_IS_CW,
    };
    gimbal_pd_tracker_t yaw_tracker;
    gimbal_pd_tracker_t pitch_tracker;
    vision_observation_t observation;
    vision_uart_stats_t uart_stats;

    (void)argument;
    g_vision_tracking_debug.task_started = 1U;
    if (!vision_uart_init()) {
        vTaskSuspend(NULL);
    }
    g_vision_tracking_debug.uart_initialized = 1U;
    gimbal_pd_tracker_init(&yaw_tracker);
    gimbal_pd_tracker_init(&pitch_tracker);
    (void)stepper_service_set_axis_enabled(STEPPER_AXIS_YAW, true);
    (void)stepper_service_set_axis_enabled(STEPPER_AXIS_PITCH, true);

    for (;;) {
        TickType_t now = xTaskGetTickCount();

        vision_uart_process((uint32_t)(now * portTICK_PERIOD_MS));
        if (vision_uart_take_latest(&observation)) {
            int32_t dx = (int32_t)observation.target_x -
                         ((int32_t)observation.frame_width / 2);
            int32_t dy = (int32_t)observation.target_y -
                         ((int32_t)observation.frame_height / 2);

            g_vision_tracking_debug.last_target_valid =
                observation.target_valid ? 1U : 0U;
            g_vision_tracking_debug.last_target_x = observation.target_x;
            g_vision_tracking_debug.last_target_y = observation.target_y;
            g_vision_tracking_debug.last_error_x_pixels = dx;
            g_vision_tracking_debug.last_error_y_pixels = dy;
            {
                int32_t output_pulses;

                if (gimbal_pd_tracker_update(&yaw_tracker, &yaw_config,
                                             observation.target_valid,
                                             (int16_t)dx,
                                             (uint32_t)(now * portTICK_PERIOD_MS),
                                             &output_pulses) &&
                    submit_axis_move(&yaw, output_pulses)) {
                    g_vision_tracking_debug.yaw_command_count++;
                }
                if (gimbal_pd_tracker_update(&pitch_tracker, &pitch_config,
                                             observation.target_valid,
                                             (int16_t)dy,
                                             (uint32_t)(now * portTICK_PERIOD_MS),
                                             &output_pulses) &&
                    submit_axis_move(&pitch, output_pulses)) {
                    g_vision_tracking_debug.pitch_command_count++;
                }
                g_vision_tracking_debug.yaw_p_term_milli_pulses =
                    yaw_tracker.last_p_term_milli_pulses;
                g_vision_tracking_debug.yaw_d_term_milli_pulses =
                    yaw_tracker.last_d_term_milli_pulses;
                g_vision_tracking_debug.yaw_output_pulses =
                    yaw_tracker.last_output_pulses;
                g_vision_tracking_debug.pitch_p_term_milli_pulses =
                    pitch_tracker.last_p_term_milli_pulses;
                g_vision_tracking_debug.pitch_d_term_milli_pulses =
                    pitch_tracker.last_d_term_milli_pulses;
                g_vision_tracking_debug.pitch_output_pulses =
                    pitch_tracker.last_output_pulses;
            }
        }
        vision_uart_get_stats(&uart_stats);
        g_vision_tracking_debug.uart_packet_count = uart_stats.valid_packet_count;
        g_vision_tracking_debug.uart_parse_error_count = uart_stats.parse_error_count;
        g_vision_tracking_debug.uart_overrun_count = uart_stats.rx_overrun_count;
        send_telemetry((uint32_t)(now * portTICK_PERIOD_MS));
        vision_uart_service_tx();
        /* Publish this loop's snapshot. Keep the sequence even for RAM polling. */
        g_vision_tracking_debug.update_sequence += 2U;
        vTaskDelay(pdMS_TO_TICKS(VISION_TRACKING_TASK_PERIOD_MS));
    }
}
