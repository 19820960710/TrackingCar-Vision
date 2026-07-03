#ifndef PID_H
#define PID_H

#include <stdint.h>

typedef struct {
    int32_t kp_milli;
    int32_t ki_milli;
    int32_t kd_milli;
    int32_t out_min;
    int32_t out_max;
    int32_t output;
    int32_t output_milli;
    int32_t err_1;
    int32_t err_2;
} pid_inc_t;

void pid_inc_init(pid_inc_t *pid, int32_t kp_milli, int32_t ki_milli, int32_t kd_milli,
                  int32_t out_min, int32_t out_max);
void pid_inc_set_gain(pid_inc_t *pid, int32_t kp_milli, int32_t ki_milli, int32_t kd_milli);
void pid_inc_reset(pid_inc_t *pid);
int32_t pid_inc_compute(pid_inc_t *pid, int32_t target, int32_t measured);
int32_t pid_inc_get_output(const pid_inc_t *pid);

#endif /* PID_H */
