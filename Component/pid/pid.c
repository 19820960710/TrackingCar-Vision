#include "pid/pid.h"

static int32_t clamp_i32(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value > max_value) {
        return max_value;
    }
    if (value < min_value) {
        return min_value;
    }
    return value;
}

static int32_t milli_to_i32_round(int32_t value_milli)
{
    if (value_milli >= 0) {
        return (value_milli + 500) / 1000;
    }
    return (value_milli - 500) / 1000;
}

void pid_inc_init(pid_inc_t *pid, int32_t kp_milli, int32_t ki_milli, int32_t kd_milli,
                  int32_t out_min, int32_t out_max)
{
    if (pid == 0) {
        return;
    }

    pid->kp_milli = kp_milli;
    pid->ki_milli = ki_milli;
    pid->kd_milli = kd_milli;
    pid->out_min = out_min;
    pid->out_max = out_max;
    pid_inc_reset(pid);
}

void pid_inc_set_gain(pid_inc_t *pid, int32_t kp_milli, int32_t ki_milli, int32_t kd_milli)
{
    if (pid == 0) {
        return;
    }

    pid->kp_milli = kp_milli;
    pid->ki_milli = ki_milli;
    pid->kd_milli = kd_milli;
}

void pid_inc_reset(pid_inc_t *pid)
{
    if (pid == 0) {
        return;
    }

    pid->output = 0;
    pid->output_milli = 0;
    pid->err_1 = 0;
    pid->err_2 = 0;
}

int32_t pid_inc_compute(pid_inc_t *pid, int32_t target, int32_t measured)
{
    if (pid == 0) {
        return 0;
    }

    int32_t err = target - measured;
    int64_t delta_milli = 0;
    int32_t min_milli = pid->out_min * 1000;
    int32_t max_milli = pid->out_max * 1000;

    delta_milli += (int64_t)pid->kp_milli * (err - pid->err_1);
    delta_milli += (int64_t)pid->ki_milli * err;
    delta_milli += (int64_t)pid->kd_milli * (err - 2 * pid->err_1 + pid->err_2);

    pid->output_milli = clamp_i32(pid->output_milli + (int32_t)delta_milli,
                                  min_milli, max_milli);
    pid->output = milli_to_i32_round(pid->output_milli);
    pid->err_2 = pid->err_1;
    pid->err_1 = err;

    return pid->output;
}

int32_t pid_inc_get_output(const pid_inc_t *pid)
{
    return (pid == 0) ? 0 : pid->output;
}
