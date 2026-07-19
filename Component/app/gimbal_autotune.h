/** @file gimbal_autotune.h @brief RAM mailbox contract for camera gimbal trials. */
#ifndef GIMBAL_AUTOTUNE_H
#define GIMBAL_AUTOTUNE_H

#include <stdint.h>

#define GIMBAL_AUTOTUNE_MAX_TRIALS 8U
#define GIMBAL_AUTOTUNE_MAX_TRACE_SAMPLES 64U

typedef enum { GIMBAL_AUTOTUNE_IDLE, GIMBAL_AUTOTUNE_RUNNING,
               GIMBAL_AUTOTUNE_COMPLETE, GIMBAL_AUTOTUNE_FAILED } gimbal_autotune_status_t;
typedef enum { GIMBAL_AUTOTUNE_PHASE_IDLE, GIMBAL_AUTOTUNE_PHASE_OFFSET,
               GIMBAL_AUTOTUNE_PHASE_WAIT_REACHED, GIMBAL_AUTOTUNE_PHASE_TRACK,
               GIMBAL_AUTOTUNE_PHASE_SETTLED } gimbal_autotune_phase_t;

typedef struct {
    uint32_t request_seq, request_seq_inv, abort_seq;
    float yaw_kp, yaw_kd, pitch_kp, pitch_kd;
    uint32_t seed, trial_count, yaw_max_pulses, pitch_max_pulses;
    uint32_t offset_timeout_ms, track_timeout_ms, target_lost_ms, stable_samples;
} gimbal_autotune_command_t;

typedef struct {
    int32_t yaw_offset, pitch_offset;
    uint32_t offset_ms, return_ms, yaw_settle_ms, pitch_settle_ms;
    uint32_t valid_samples, target_lost_samples;
    float yaw_max_error, yaw_stable_mean_error;
    uint32_t yaw_zero_crossings, yaw_max_pulse_step;
    float pitch_max_error, pitch_stable_mean_error;
    uint32_t pitch_zero_crossings, pitch_max_pulse_step;
    int32_t yaw_position_before, yaw_position_after;
    int32_t pitch_position_before, pitch_position_after;
    uint32_t yaw_response_function, yaw_response_code;
    uint32_t pitch_response_function, pitch_response_code;
    uint32_t status;
} gimbal_autotune_trial_result_t;

typedef struct {
    uint32_t trial, elapsed_ms, target_valid;
    int32_t dx, dy, yaw_pulses, pitch_pulses;
} gimbal_autotune_trace_t;

typedef struct {
    volatile uint32_t snapshot_seq, ack_seq, status, phase, current_trial, failed_trials;
    volatile uint32_t trace_count, trace_overflow;
    volatile float applied_yaw_kp, applied_yaw_kd, applied_pitch_kp, applied_pitch_kd;
    volatile uint32_t observed_target_valid;
    volatile int32_t observed_dx, observed_dy;
    volatile gimbal_autotune_trial_result_t result[GIMBAL_AUTOTUNE_MAX_TRIALS];
    volatile gimbal_autotune_trace_t trace[GIMBAL_AUTOTUNE_MAX_TRACE_SAMPLES];
    volatile gimbal_autotune_command_t command;
} gimbal_autotune_mailbox_t;

extern volatile gimbal_autotune_mailbox_t g_gimbal_autotune;
void gimbal_autotune_task(void *argument);

#endif
