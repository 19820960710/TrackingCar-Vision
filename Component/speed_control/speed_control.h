#ifndef SPEED_CONTROL_H
#define SPEED_CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include "pid/pid.h"

#define SPEED_CONTROL_PERIOD_MS 50U

typedef struct {
    int32_t left_rpm;
    int32_t right_rpm;
    bool low_speed_ff_enable;
} speed_target_msg_t;

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
} speed_loop_context_t;

bool speed_control_queue_init(void);
bool speed_control_set_target(int32_t left_rpm, int32_t right_rpm);
bool speed_control_set_target_with_ff(int32_t left_rpm,
                                      int32_t right_rpm,
                                      bool low_speed_ff_enable);
bool speed_control_receive_target(speed_target_msg_t *target);
void speed_control_publish_state(const speed_loop_context_t *ctx);
bool speed_control_wheels_stopped_snapshot(void);

void speed_control_pid_init(pid_inc_t *left_pid, pid_inc_t *right_pid);
int32_t speed_control_ramp_step(int32_t current, int32_t target);
int32_t speed_control_apply_start_feedforward(int32_t pwm,
                                              int32_t setpoint_rpm,
                                              int32_t measured_rpm,
                                              bool enable);

void speed_loop_init(speed_loop_context_t *ctx);
void speed_loop_set_target(speed_loop_context_t *ctx, int32_t left_rpm, int32_t right_rpm);
void speed_loop_update(speed_loop_context_t *ctx, bool start_ff_enable);
bool speed_loop_is_wheels_stopped(const speed_loop_context_t *ctx);

#endif /* SPEED_CONTROL_H */
