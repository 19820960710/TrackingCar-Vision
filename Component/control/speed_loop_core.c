/**
 * @file    speed_loop_core.c
 * @brief   速度环纯算法核心实现：编码器 delta→RPM 转换、一阶低通滤波、
 *          目标斜坡、增量式 PID、低速前馈与 PWM 输出计算。
 *
 * @details 本文件不依赖 FreeRTOS、编码器驱动或 TB6612，仅做纯运算。
 *          硬件适配由 speed_service 负责：读取编码器增量传入，取输出 PWM 写入电机驱动。
 *
 *          节拍约定：10ms 采样，5 次累计一个 50ms 窗口做 PID。
 *          单位约定：RPM 为整数，内部滤波用 RPM×10 保留精度；PWM 为整数。
 *
 *          关键算法：
 *          - delta_to_rpm10(): 编码器增量转 RPM×10，使用 int64_t 防溢出
 *          - 一阶低通滤波：y += (x - y) / 2，系数 = 0.5，截止频率约 1/(2πτ)
 *          - 目标斜坡：每周期最多变化 100 RPM，减少正反转切换冲击
 *          - 增量式 PID：天然抗积分饱和，仅输出变化量
 *          - 低速前馈：克服静摩擦，设定点/误差/比例衰减三重条件判断
 */
#include "control/speed_loop_core.h"
#include <stddef.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  速度环可调参数区
 *
 *  修改参数后重新编译即可生效。
 * ═══════════════════════════════════════════════════════════════════════════ */
#define PID_DEFAULT_KP_MILLI             80    /* Kp = 0.080，速度环比例增益 */
#define PID_DEFAULT_KI_MILLI             50    /* Ki = 0.050，速度环积分增益 */
#define PID_DEFAULT_KD_MILLI             0     /* Kd = 0，微分项设为 0 避免放大量化噪声 */
#define PID_OUTPUT_MIN                   (-80) /* PWM 输出下限（反转最大占空比） */
#define PID_OUTPUT_MAX                   (80)  /* PWM 输出上限（正转最大占空比） */
#define SPEED_RAMP_STEP_RPM              100   /* 目标斜坡步长：每 50ms 最多变化 100 RPM */

/* 低速前馈参数（克服静摩擦） */
#define SPEED_START_FF_PWM               30    /* 最大前馈 PWM 值 */
#define SPEED_START_FF_SETPOINT_RPM      90    /* 设定点超过此值不启用前馈（高速时不需要） */
#define SPEED_START_FF_ERR_RPM           2     /* 误差小于此值不启用前馈（已接近目标） */
#define SPEED_START_FF_MIN_SETPOINT_RPM  4     /* 设定点低于此值不启用前馈（过小） */
#define SPEED_START_FF_FULL_SETPOINT_RPM 24    /* 设定点在此值以内前馈按比例衰减 */
#define SPEED_START_FF_MIN_PWM           12    /* 前馈最小值，用于比例衰减的下界 */


/**
 * @brief  取 32 位整数绝对值。
 */
static int32_t abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

/**
 * @brief  编码器增量转 RPM×10（保留一位小数精度）。
 *
 *         公式：RPM10 = delta × 600000 / (counts_per_rev × period_ms)
 *         其中 600000 = 60 s/min × 1000 ms/s × 10（RPM 一位小数）
 *
 *         使用 int64_t 中间变量防止乘法溢出：
 *         - 最大 delta 约 ±2000（编码器分辨率 × 电机最高转速）
 *         - 600000 × 2000 = 1.2e9，超出 int32 范围（2.1e9），但留安全余量
 *
 * @param  delta           编码器在该周期内的脉冲增量（带方向）
 * @param  period_ms       累计采样周期 (ms)
 * @param  counts_per_rev  编码器每转脉冲数（4 倍频后）
 * @return RPM×10，正值=正转，负值=反转
 */
static int32_t delta_to_rpm10(int32_t delta,
                              uint32_t period_ms,
                              uint32_t counts_per_rev)
{
    if (period_ms == 0U || counts_per_rev == 0U) {
        return 0;
    }
    return (int32_t)(((int64_t)delta * 600000) /
                     ((int64_t)counts_per_rev * period_ms));
}

/**
 * @brief  左右轮 PID 装入默认参数。
 *         Kd=0 避免编码器量化噪声经微分放大导致 PWM 抖动。
 */
static void pid_init(pid_inc_t *left_pid, pid_inc_t *right_pid)
{
    pid_inc_init(left_pid, PID_DEFAULT_KP_MILLI, PID_DEFAULT_KI_MILLI,
                 PID_DEFAULT_KD_MILLI, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    pid_inc_init(right_pid, PID_DEFAULT_KP_MILLI, PID_DEFAULT_KI_MILLI,
                 PID_DEFAULT_KD_MILLI, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
}

/**
 * @brief  目标速度斜坡：每周期向 target 最多变化 SPEED_RAMP_STEP_RPM。
 *         防止直接下大目标时 PWM 阶跃导致电机电流冲击或小车"窜动"。
 *
 * @param  current  当前斜坡输出值
 * @param  target   最终目标值
 * @return 斜坡后的新值
 */
static int32_t ramp_step(int32_t current, int32_t target)
{
    int32_t step = SPEED_RAMP_STEP_RPM;

    if (current < target) {
        current += step;
        if (current > target) {
            current = target;
        }
    } else if (current > target) {
        current -= step;
        if (current < target) {
            current = target;
        }
    }

    return current;
}

/**
 * @brief  低速启动前馈（克服静摩擦）。
 *
 *         直流电机在低速时因静摩擦力无法启动，PID 积分需要较长时间积累。
 *         本函数在 PID 输出基础上叠加一个开环前馈量，帮助电机克服静摩擦。
 *
 *         前馈生效条件（全部满足）：
 *         1. enable=true 且前馈值 > 0
 *         2. 设定点非零且不超过 SPEED_START_FF_SETPOINT_RPM（高速区不需要）
 *         3. 设定点绝对值大于等于 SPEED_START_FF_MIN_SETPOINT_RPM（太小不触发）
 *         4. 实测速度与设定点误差大于 SPEED_START_FF_ERR_RPM（已接近时退出）
 *
 *         设定点处于 (MIN, FULL] 区间时，前馈按比例衰减，避免小目标严重过冲。
 *
 * @param  pwm            PID 计算输出的 PWM 值
 * @param  setpoint_rpm   斜坡后的设定点 (RPM)
 * @param  measured_rpm   实测速度 (RPM)
 * @param  enable         是否允许前馈（由 yaw 控制层决定）
 * @return 叠加前馈后的 PWM 值
 */
static int32_t apply_start_feedforward(int32_t pwm,
                                       int32_t setpoint_rpm,
                                       int32_t measured_rpm,
                                       bool enable)
{
    int32_t ff_pwm = SPEED_START_FF_PWM;
    if (ff_pwm < 0) {
        ff_pwm = -ff_pwm;
    }

    int32_t abs_setpoint = abs_i32(setpoint_rpm);
    if (!enable || ff_pwm == 0 || setpoint_rpm == 0 ||
        abs_setpoint > SPEED_START_FF_SETPOINT_RPM ||
        abs_setpoint < SPEED_START_FF_MIN_SETPOINT_RPM ||
        abs_i32(setpoint_rpm - measured_rpm) <= SPEED_START_FF_ERR_RPM) {
        return pwm;
    }

    /* 设定点较低时按比例衰减前馈，防止小目标严重过冲 */
    if (abs_setpoint < SPEED_START_FF_FULL_SETPOINT_RPM) {
        int32_t ff_span = ff_pwm - SPEED_START_FF_MIN_PWM;
        if (ff_span > 0) {
            ff_pwm = SPEED_START_FF_MIN_PWM +
                (ff_span * abs_setpoint) / SPEED_START_FF_FULL_SETPOINT_RPM;
        }
    }

    /* 仅当 PID 输出方向与设定点方向一致（或为 0）时才替换，
     * 防止前馈对抗 PID（如 PID 正在减速但前馈要求加速）。 */
    int32_t sign = (setpoint_rpm > 0) ? 1 : -1;
    if (pwm != 0 && ((pwm > 0 && sign < 0) || (pwm < 0 && sign > 0))) {
        return pwm;
    }
    if (abs_i32(pwm) >= ff_pwm) {
        return pwm;
    }
    return (sign > 0) ? ff_pwm : -ff_pwm;
}


void speed_loop_core_init(speed_loop_core_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    /* 清零整个结构体，然后初始化 PID */
    speed_loop_core_t init = {0};
    *ctx = init;
    pid_init(&ctx->left_pid, &ctx->right_pid);
}


bool speed_loop_core_set_target(speed_loop_core_t *ctx,
                                int32_t left_rpm,
                                int32_t right_rpm)
{
    if (ctx == NULL) {
        return false;
    }

    ctx->left_target_rpm = left_rpm;
    ctx->right_target_rpm = right_rpm;

    /* 目标为 0/0：表示要求立即停止。
     * 立即清零斜坡设定点、PWM 输出，并复位 PID，
     * 返回 true 提示调用方应执行制动（电机短接）。 */
    if (left_rpm == 0 && right_rpm == 0) {
        ctx->left_setpoint_rpm = 0;
        ctx->right_setpoint_rpm = 0;
        ctx->left_pwm = 0;
        ctx->right_pwm = 0;
        pid_inc_reset(&ctx->left_pid);
        pid_inc_reset(&ctx->right_pid);
        return true;
    }

    return false;
}


void speed_loop_core_get_output(const speed_loop_core_t *ctx,
                                speed_loop_core_output_t *out)
{
    if (ctx == NULL || out == NULL) {
        return;
    }

    out->left_rpm = ctx->left_rpm;
    out->right_rpm = ctx->right_rpm;
    out->left_target_rpm = ctx->left_target_rpm;
    out->right_target_rpm = ctx->right_target_rpm;
    out->left_pwm = ctx->left_pwm;
    out->right_pwm = ctx->right_pwm;
    out->stopped = speed_loop_core_is_stopped(ctx);
    out->brake = out->stopped;
}


bool speed_loop_core_update(speed_loop_core_t *ctx,
                            int32_t left_delta,
                            int32_t right_delta,
                            uint32_t sample_period_ms,
                            uint32_t counts_per_rev,
                            bool start_ff_enable,
                            speed_loop_core_output_t *out)
{
    if (ctx == NULL) {
        return false;
    }

    /* ────── 累计编码器增量 ────── */
    ctx->left_delta_sum += left_delta;
    ctx->right_delta_sum += right_delta;
    ctx->sample_count++;

    /* ────── 未满 50ms 窗口：不执行 PID，直接返回当前输出 ────── */
    if (ctx->sample_count <
        (SPEED_LOOP_CORE_CONTROL_PERIOD_MS / SPEED_LOOP_CORE_SAMPLE_PERIOD_MS)) {
        if (out != NULL) {
            speed_loop_core_get_output(ctx, out);
        }
        return false;
    }

    /* ────── 窗口满（50ms）：执行完整的 PID 计算 ────── */

    /* 1. 计算累计窗口内的等效 RPM×10，注意 period_ms 是累计采样 ms */
    uint32_t period_ms = ctx->sample_count * sample_period_ms;
    int32_t left_rpm10 = delta_to_rpm10(ctx->left_delta_sum,
                                        period_ms,
                                        counts_per_rev);
    int32_t right_rpm10 = delta_to_rpm10(ctx->right_delta_sum,
                                         period_ms,
                                         counts_per_rev);

    /* 2. 一阶低通滤波：y[n] = y[n-1] + (x[n] - y[n-1]) / 2
     *    等效 RC 低通，截止频率 fc = 1/(2πτ)，其中 τ = 周期 × (系数倒数) ≈ 100ms
     *    目的：滤除编码器量化噪声和齿槽效应导致的 RPM 波动 */
    ctx->left_rpm10_filt += (left_rpm10 - ctx->left_rpm10_filt) / 2;
    ctx->right_rpm10_filt += (right_rpm10 - ctx->right_rpm10_filt) / 2;

    /* 3. 清零累计器，准备下一个窗口 */
    ctx->left_delta_sum = 0;
    ctx->right_delta_sum = 0;
    ctx->sample_count = 0;

    /* 4. 目标斜坡：让设定点缓慢逼近目标，防止阶跃 */
    ctx->left_setpoint_rpm = ramp_step(ctx->left_setpoint_rpm,
                                       ctx->left_target_rpm);
    ctx->right_setpoint_rpm = ramp_step(ctx->right_setpoint_rpm,
                                        ctx->right_target_rpm);

    /* 5. 滤波后的 RPM×10 取整为整数 RPM */
    ctx->left_rpm = ctx->left_rpm10_filt / 10;
    ctx->right_rpm = ctx->right_rpm10_filt / 10;

    /* 6. 增量式 PID 计算：输入 = 设定点 - 实测值（误差） */
    ctx->left_pwm = pid_inc_compute(&ctx->left_pid,
                                    ctx->left_setpoint_rpm,
                                    ctx->left_rpm);
    ctx->right_pwm = pid_inc_compute(&ctx->right_pid,
                                     ctx->right_setpoint_rpm,
                                     ctx->right_rpm);

    /* 7. 叠加低速启动前馈（克服静摩擦） */
    ctx->left_pwm = apply_start_feedforward(ctx->left_pwm,
        ctx->left_setpoint_rpm, ctx->left_rpm, start_ff_enable);
    ctx->right_pwm = apply_start_feedforward(ctx->right_pwm,
        ctx->right_setpoint_rpm, ctx->right_rpm, start_ff_enable);

    /* 8. 目标与设定点均归零时的额外安全处理：清 PWM 并复位 PID
     *    确保惯性滑行结束后完全停稳，防止积分残值导致意外微动 */
    if (ctx->left_target_rpm == 0 && ctx->right_target_rpm == 0 &&
        ctx->left_setpoint_rpm == 0 && ctx->right_setpoint_rpm == 0) {
        ctx->left_pwm = 0;
        ctx->right_pwm = 0;
        pid_inc_reset(&ctx->left_pid);
        pid_inc_reset(&ctx->right_pid);
    }

    if (out != NULL) {
        speed_loop_core_get_output(ctx, out);
    }
    return true;   /* 本周期完成了 PID 计算，调用方应输出 PWM 到电机 */
}


bool speed_loop_core_is_stopped(const speed_loop_core_t *ctx)
{
    if (ctx == NULL) {
        return true;
    }
    /* 全部归零才算停稳 */
    return (ctx->left_target_rpm == 0 && ctx->right_target_rpm == 0 &&
            ctx->left_setpoint_rpm == 0 && ctx->right_setpoint_rpm == 0 &&
            ctx->left_pwm == 0 && ctx->right_pwm == 0);
}
