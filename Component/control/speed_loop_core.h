#ifndef SPEED_LOOP_CORE_H
#define SPEED_LOOP_CORE_H

#include "pid/pid.h"
#include <stdbool.h>
#include <stdint.h>

#define SPEED_LOOP_CORE_CONTROL_PERIOD_MS 50U
#define SPEED_LOOP_CORE_SAMPLE_PERIOD_MS  10U

typedef struct {
    pid_inc_t left_pid;
    pid_inc_t right_pid;
    int32_t left_rpm10_filt;
    int32_t right_rpm10_filt;
    int32_t left_target_rpm;
    int32_t right_target_rpm;
    int32_t left_setpoint_rpm;
    int32_t right_setpoint_rpm;
    int32_t left_delta_sum;
    int32_t right_delta_sum;
    uint32_t sample_count;
    int32_t left_rpm;
    int32_t right_rpm;
    int32_t left_pwm;
    int32_t right_pwm;
} speed_loop_core_t;

typedef struct {
    int32_t left_rpm;
    int32_t right_rpm;
    int32_t left_target_rpm;
    int32_t right_target_rpm;
    int32_t left_pwm;
    int32_t right_pwm;
    bool stopped;
    bool brake;
} speed_loop_core_output_t;

void speed_loop_core_init(speed_loop_core_t *ctx);
bool speed_loop_core_set_target(speed_loop_core_t *ctx,
                                int32_t left_rpm,
                                int32_t right_rpm);
bool speed_loop_core_update(speed_loop_core_t *ctx,
                            int32_t left_delta,
                            int32_t right_delta,
                            uint32_t sample_period_ms,
                            uint32_t counts_per_rev,
                            bool start_ff_enable,
                            speed_loop_core_output_t *out);
void speed_loop_core_get_output(const speed_loop_core_t *ctx,
                                speed_loop_core_output_t *out);
bool speed_loop_core_is_stopped(const speed_loop_core_t *ctx);

#endif /* SPEED_LOOP_CORE_H */
