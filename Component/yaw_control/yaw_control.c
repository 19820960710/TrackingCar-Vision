#include "yaw_control/yaw_control.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  yaw 可调参数区
 *  单位约定：角度使用 deg10，即 0.1°；PID 系数使用 milli，即 ×1000。
 * ═══════════════════════════════════════════════════════════════════════════ */
#define YAW_LOOP_PERIOD_MS             50
#define YAW_PID_DEFAULT_KP_MILLI       120
#define YAW_PID_DEFAULT_KI_MILLI       0
#define YAW_PID_DEFAULT_KD_MILLI       170
#define YAW_PID_OUTPUT_LIMIT_RPM       200
#define YAW_DYNAMIC_CAP_BASE_RPM       18
#define YAW_DYNAMIC_CAP_ERR_DIV        14
#define YAW_MIN_TURN_RPM               7    /* 死区外给一次明确修正，避免小碎步 */
#define YAW_DEADBAND_DEG10             12   /* 1.2° 内认为到位 */
#define YAW_REACQUIRE_DEG10            25   /* 到位后需超过 2.5° 才重新修正 */
#define YAW_REACQUIRE_CONFIRM_COUNT    2U   /* 连续 2 个 yaw 周期超出阈值才重捕获 */
#define YAW_INTEGRAL_ZONE_DEG10        120  /* 12° 内才积分 */
#define YAW_INTEGRAL_LIMIT_RPM         0    /* 保持态默认不用积分追尾差 */
#define YAW_MIN_TURN_ZONE_DEG10        180  /* 18° 内启用连续恢复速度曲线 */
#define YAW_TARGET_RAMP_STEP_DEG10     150  /* 目标斜坡步长，0.1°/50ms */
#define YAW_RECOVER_MAX_TURN_RPM       15   /* 小误差恢复曲线最大转向速度 */
#define YAW_APPROACH_BRAKE_ZONE_DEG10  120  /* 12° 内快速靠近则清零制动 */
#define YAW_APPROACH_DERR_DEG10        2    /* 0.2°/50ms 以上认为明显靠近 */

int32_t yaw_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

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

int32_t yaw_normalize_deg10(int32_t angle_deg10)
{
    while (angle_deg10 > 1800) {
        angle_deg10 -= 3600;
    }
    while (angle_deg10 <= -1800) {
        angle_deg10 += 3600;
    }
    return angle_deg10;
}

int32_t yaw_float_deg_to_deg10(float angle_deg)
{
    if (angle_deg >= 0.0f) {
        return (int32_t)(angle_deg * 10.0f + 0.5f);
    }
    return (int32_t)(angle_deg * 10.0f - 0.5f);
}

float yaw_normalize_deg(float angle)
{
    while (angle > 180.0f) {
        angle -= 360.0f;
    }
    while (angle <= -180.0f) {
        angle += 360.0f;
    }
    return angle;
}

static int32_t yaw_target_ramp_step(int32_t current_deg10,
                                    int32_t target_deg10,
                                    int32_t step_deg10)
{
    int32_t delta = yaw_normalize_deg10(target_deg10 - current_deg10);

    if (step_deg10 < 0) {
        step_deg10 = -step_deg10;
    }
    if (step_deg10 == 0 || yaw_abs_i32(delta) <= step_deg10) {
        return yaw_normalize_deg10(target_deg10);
    }

    if (delta > 0) {
        current_deg10 += step_deg10;
    } else {
        current_deg10 -= step_deg10;
    }
    return yaw_normalize_deg10(current_deg10);
}

static int32_t yaw_recover_turn_for_error(int32_t abs_err_deg10,
                                          int32_t deadband_deg10,
                                          int32_t zone_deg10,
                                          int32_t min_turn_rpm)
{
    if (min_turn_rpm <= 0 || abs_err_deg10 <= deadband_deg10) {
        return 0;
    }
    int32_t max_turn = YAW_RECOVER_MAX_TURN_RPM;
    if (zone_deg10 <= deadband_deg10 || abs_err_deg10 >= zone_deg10) {
        return max_turn;
    }

    int32_t span = zone_deg10 - deadband_deg10;
    int32_t pos = abs_err_deg10 - deadband_deg10;
    return min_turn_rpm + ((max_turn - min_turn_rpm) * pos) / span;
}

void yaw_control_init(yaw_control_t *control)
{
    if (control == 0) {
        return;
    }
    pid_pos_init(&control->pid, YAW_PID_DEFAULT_KP_MILLI,
                 YAW_PID_DEFAULT_KI_MILLI, YAW_PID_DEFAULT_KD_MILLI,
                 -YAW_PID_OUTPUT_LIMIT_RPM, YAW_PID_OUTPUT_LIMIT_RPM);
    control->control_target_deg10 = 0;
    control->control_target_initialized = false;
    control->settled_latch = false;
    control->reacquire_count = 0;
}

void yaw_control_reset(yaw_control_t *control)
{
    if (control == 0) {
        return;
    }
    pid_pos_reset(&control->pid);
    control->control_target_initialized = false;
    control->settled_latch = false;
    control->reacquire_count = 0;
}

static int32_t yaw_pid_compute_turn(pid_pos_t *pid,
                                    int32_t err_deg10,
                                    bool allow_static_boost,
                                    int32_t *derr_out,
                                    bool *approaching_close_out)
{
    if (derr_out != 0) {
        *derr_out = 0;
    }
    if (approaching_close_out != 0) {
        *approaching_close_out = false;
    }
    if (pid == 0) {
        return 0;
    }

    int32_t abs_err = yaw_abs_i32(err_deg10);
    int32_t deadband = YAW_DEADBAND_DEG10;
    int32_t min_turn = YAW_MIN_TURN_RPM;
    int32_t izone = YAW_INTEGRAL_ZONE_DEG10;
    int32_t ilimit = YAW_INTEGRAL_LIMIT_RPM;

    if (izone < deadband) izone = deadband;
    if (ilimit < 0) ilimit = -ilimit;

    if (abs_err <= deadband) {
        pid_pos_reset(pid);
        return 0;
    }

    int32_t derr = 0;
    if (pid->first_run) {
        pid->first_run = 0U;
    } else {
        derr = err_deg10 - pid->last_err;
    }
    if (derr_out != 0) {
        *derr_out = derr;
    }

    bool approaching_target =
        ((err_deg10 > 0 && derr < 0) || (err_deg10 < 0 && derr > 0));
    bool approaching_fast = approaching_target &&
        (yaw_abs_i32(derr) >= YAW_APPROACH_DERR_DEG10);
    if (approaching_close_out != 0) {
        *approaching_close_out = approaching_fast &&
            abs_err <= YAW_APPROACH_BRAKE_ZONE_DEG10;
    }

    if (abs_err <= izone && pid->ki_milli != 0) {
        int64_t next_integral = (int64_t)pid->integral_milli +
                                (int64_t)pid->ki_milli * err_deg10;
        int32_t int_limit_milli = ilimit * 1000;
        if (next_integral > int_limit_milli) {
            pid->integral_milli = int_limit_milli;
        } else if (next_integral < -int_limit_milli) {
            pid->integral_milli = -int_limit_milli;
        } else {
            pid->integral_milli = (int32_t)next_integral;
        }
    } else {
        pid->integral_milli /= 2;
    }
    if (approaching_fast && abs_err <= YAW_APPROACH_BRAKE_ZONE_DEG10) {
        pid->integral_milli /= 2;
    }

    int64_t output_milli = 0;
    output_milli += (int64_t)pid->kp_milli * err_deg10;
    output_milli += pid->integral_milli;
    output_milli += (int64_t)pid->kd_milli * derr;

    int32_t out_min_milli = pid->out_min * 1000;
    int32_t out_max_milli = pid->out_max * 1000;
    if (output_milli > out_max_milli) {
        output_milli = out_max_milli;
    } else if (output_milli < out_min_milli) {
        output_milli = out_min_milli;
    }

    int32_t output = milli_to_i32_round((int32_t)output_milli);
    int32_t dynamic_limit = YAW_DYNAMIC_CAP_BASE_RPM +
                            (abs_err / YAW_DYNAMIC_CAP_ERR_DIV);
    if (dynamic_limit < min_turn) {
        dynamic_limit = min_turn;
    }
    if (dynamic_limit > pid->out_max) {
        dynamic_limit = pid->out_max;
    }
    output = clamp_i32(output, -dynamic_limit, dynamic_limit);

    if (approaching_fast && abs_err <= YAW_APPROACH_BRAKE_ZONE_DEG10) {
        output = 0;
    }

    int32_t err_sign = (err_deg10 > 0) ? 1 : -1;
    int32_t recover_turn = yaw_recover_turn_for_error(abs_err,
                                                       deadband,
                                                       YAW_MIN_TURN_ZONE_DEG10,
                                                       min_turn);
    bool output_same_direction = (output == 0) ||
        ((output > 0 && err_sign > 0) || (output < 0 && err_sign < 0));
    if (allow_static_boost && recover_turn > 0 &&
        abs_err <= YAW_MIN_TURN_ZONE_DEG10 &&
        !(approaching_fast && abs_err <= YAW_APPROACH_BRAKE_ZONE_DEG10) &&
        output_same_direction && yaw_abs_i32(output) < recover_turn) {
        output = (err_sign > 0) ? recover_turn : -recover_turn;
    }

    output = clamp_i32(output, pid->out_min, pid->out_max);
    pid->output = output;
    pid->last_err = err_deg10;
    return output;
}

void yaw_control_update(yaw_control_t *control,
                        int32_t target_yaw_deg10,
                        int32_t current_yaw_deg10,
                        yaw_control_output_t *output)
{
    if (output == 0) {
        return;
    }

    output->turn_rpm = 0;
    output->control_error_deg10 = 0;
    output->derr_deg10 = 0;
    output->settled = false;
    output->speed_ff_enable = false;
    output->approaching_close = false;

    if (control == 0) {
        return;
    }

    if (!control->control_target_initialized) {
        control->control_target_deg10 = current_yaw_deg10;
        control->control_target_initialized = true;
    }
    control->control_target_deg10 = yaw_target_ramp_step(
        control->control_target_deg10,
        target_yaw_deg10,
        YAW_TARGET_RAMP_STEP_DEG10);

    output->control_error_deg10 = yaw_normalize_deg10(
        control->control_target_deg10 - current_yaw_deg10);
    int32_t abs_control_error = yaw_abs_i32(output->control_error_deg10);

    if (control->settled_latch) {
        if (abs_control_error >= YAW_REACQUIRE_DEG10) {
            if (control->reacquire_count < YAW_REACQUIRE_CONFIRM_COUNT) {
                control->reacquire_count++;
            }
            if (control->reacquire_count >= YAW_REACQUIRE_CONFIRM_COUNT) {
                control->settled_latch = false;
                control->reacquire_count = 0;
                pid_pos_reset(&control->pid);
            }
        } else {
            control->reacquire_count = 0;
        }
    }

    if (control->settled_latch) {
        pid_pos_reset(&control->pid);
        output->turn_rpm = 0;
        output->settled = true;
        output->approaching_close = true;
        return;
    }

    bool allow_static_boost = (control->control_target_deg10 == target_yaw_deg10);
    output->turn_rpm = yaw_pid_compute_turn(&control->pid,
                                            output->control_error_deg10,
                                            allow_static_boost,
                                            &output->derr_deg10,
                                            &output->approaching_close);

    if (abs_control_error <= YAW_DEADBAND_DEG10 ||
        (output->turn_rpm == 0 && output->approaching_close &&
         abs_control_error < YAW_REACQUIRE_DEG10)) {
        control->settled_latch = true;
        control->reacquire_count = 0;
        pid_pos_reset(&control->pid);
        output->turn_rpm = 0;
    }

    output->settled = control->settled_latch;
    output->speed_ff_enable = (!output->settled && output->turn_rpm != 0 &&
                               abs_control_error > YAW_DEADBAND_DEG10 &&
                               !output->approaching_close);
}
