#ifndef LINE_TRACKING_H
#define LINE_TRACKING_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LINE_TRACKING_MODE_TRACKING = 0,
    LINE_TRACKING_MODE_CORNER_ALIGNMENT,
    LINE_TRACKING_MODE_LOSS_TURN_CW,
    LINE_TRACKING_MODE_LOSS_BRAKED
} line_tracking_mode_t;

typedef struct {
    uint32_t sample_count;
    uint32_t read_failure_count;
    uint32_t adc_timeout_count;
    uint16_t raw[8];
    uint16_t normalized[8];
    uint16_t line_peak_strength;
    uint8_t line_detected;
    float line_error;
    float left_target_mm_s;
    float right_target_mm_s;
    int32_t loss_turn_left_progress;
    int32_t loss_turn_right_progress;
    uint32_t loss_turn_count;
    uint32_t corner_event_count;
    uint32_t corner_turn_count;
    uint32_t corner_abort_count;
    line_tracking_mode_t mode;
} line_tracking_status_t;

extern volatile line_tracking_status_t g_line_tracking_status;

bool line_tracking_init(void);
void line_tracking_task(void *pvParameters);

#endif
