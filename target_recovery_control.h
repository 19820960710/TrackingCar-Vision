#ifndef TARGET_RECOVERY_CONTROL_H
#define TARGET_RECOVERY_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TARGET_RECOVERY_TRACKING = 0U,
    TARGET_RECOVERY_PREDICTING,
    TARGET_RECOVERY_SCANNING,
} target_recovery_mode_t;

typedef struct {
    uint32_t prediction_hold_ms;
    uint32_t scan_command_period_ms;
    int32_t yaw_scan_half_span_pulses;
    int32_t pitch_scan_half_span_pulses;
    int32_t yaw_scan_step_pulses;
    int32_t pitch_scan_step_pulses;
} target_recovery_config_t;

typedef struct {
    const target_recovery_config_t *config;
    target_recovery_mode_t mode;
    uint32_t target_lost_at_ms;
    uint32_t yaw_last_scan_command_ms;
    uint32_t pitch_last_scan_command_ms;
    int16_t last_valid_dx;
    int16_t last_valid_dy;
    int32_t yaw_scan_position_pulses;
    int32_t pitch_scan_position_pulses;
    int8_t yaw_scan_direction;
    int8_t pitch_scan_direction;
    bool has_valid_target_history;
} target_recovery_control_t;

typedef struct {
    target_recovery_mode_t mode;
    bool use_tracker;
    bool reset_trackers;
    bool stop_trackers;
    int16_t tracking_dx;
    int16_t tracking_dy;
    int32_t yaw_scan_pulses;
    int32_t pitch_scan_pulses;
} target_recovery_output_t;

void target_recovery_control_init(
    target_recovery_control_t *control,
    const target_recovery_config_t *config);
void target_recovery_control_update(
    target_recovery_control_t *control, bool target_valid,
    int16_t dx, int16_t dy, uint32_t now_ms,
    target_recovery_output_t *output);
void target_recovery_commit_scan(target_recovery_control_t *control,
                                 int32_t yaw_pulses, bool yaw_accepted,
                                 int32_t pitch_pulses,
                                 bool pitch_accepted,
                                 uint32_t now_ms);

#endif /* TARGET_RECOVERY_CONTROL_H */
