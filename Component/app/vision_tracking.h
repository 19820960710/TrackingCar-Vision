/**
 * @file vision_tracking.h
 * @brief FreeRTOS application task that turns MaixCAM observations into moves.
 */
#ifndef VISION_TRACKING_H
#define VISION_TRACKING_H

#include <stdint.h>

/**
 * @brief Read-only RAM snapshot, updated only by vision_tracking_task.
 *
 * update_sequence advances by two after each completed task update. This
 * permits a debugger to reject snapshots that changed during a RAM read
 * without suspending the control task.
 */
typedef struct {
    uint32_t update_sequence;
    uint32_t uart_packet_count;
    uint32_t uart_parse_error_count;
    uint32_t uart_overrun_count;
    uint32_t last_target_valid;
    uint32_t last_target_x;
    uint32_t last_target_y;
    int32_t last_error_x_pixels;
    int32_t last_error_y_pixels;
    int32_t yaw_p_term_milli_pulses;
    int32_t yaw_d_term_milli_pulses;
    int32_t yaw_output_pulses;
    int32_t pitch_p_term_milli_pulses;
    int32_t pitch_d_term_milli_pulses;
    int32_t pitch_output_pulses;
    uint32_t yaw_command_count;
    uint32_t pitch_command_count;
    uint32_t task_started;
    uint32_t uart_initialized;
} vision_tracking_debug_t;

extern volatile vision_tracking_debug_t g_vision_tracking_debug;

void vision_tracking_task(void *argument);

#endif /* VISION_TRACKING_H */
