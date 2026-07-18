/* ============================================================================
 *   闲鱼定制 小研分享屋
 *   任何非闲鱼小研分享屋出售的均为盗版
 *   正式比赛代码绑定机器绑定芯片，任何二手出售均无效
 *   请认准正版
 * ============================================================================ */

/**
 * @file    yaw_control.c
 * @brief   yaw 角环纯算法实现：目标斜坡、位置式 PID、动态限幅、到位锁存与重捕获滞回。
 *
 * @details 单位约定：角度 deg10（0.1°），PID 系数 milli（×1000）。
 *          由 yaw_loop_service 每 50ms 调用一次 yaw_control_update()。
 *
 *          控制策略要点：
 *          - 目标斜坡（150 个 0.1°/50ms = 15°/s）平滑过渡，避免阶跃冲击；
 *          - 动态限幅：误差越大允许越大的转向速度，但不低于最小修正值；
 *          - 死区外给明确最小修正，避免低于静摩擦力的"小碎步"；
 *          - 接近目标且误差快速变小时清零输出，交给速度环短接制动；
 *          - 到位锁存（进入死区后保持）+ 重捕获确认（连续 2 周期超阈值才重新修正），
 *            过滤 MPU 噪声与机械回弹导致的反复进/出死区。
 */
#include "control/yaw_control.h"
#include "common/util.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  yaw 可调参数区
 *
 *  所有角度以 deg10 为单位（0.1°），PID 系数以 milli 为单位（×1000）。
 *  修改此处参数后重新编译即可生效。
 * ═══════════════════════════════════════════════════════════════════════════ */
#define YAW_LOOP_PERIOD_MS             50     /* yaw 控制周期，与 speed 环 50ms 对齐 */
#define YAW_PID_DEFAULT_KP_MILLI       120    /* Kp = 0.120，比例增益 */
#define YAW_PID_DEFAULT_KI_MILLI       2      /* Ki = 0.002，积分增益（极小，仅消稳态误差） */
#define YAW_PID_DEFAULT_KD_MILLI       200    /* Kd = 0.200，微分增益（抑制超调） */
#define YAW_PID_OUTPUT_LIMIT_RPM       200    /* PID 输出限幅 ±200 RPM */

/* 动态限幅参数：限幅值 = 基础值 + 绝对误差 / 除数 */
#define YAW_DYNAMIC_CAP_BASE_RPM       18     /* 动态限幅基础值，最小修正也不会低于此值 */
#define YAW_DYNAMIC_CAP_ERR_DIV        14     /* 误差除数：误差每 14 个 0.1° 增加 1 RPM */

#define YAW_MIN_TURN_RPM               15     /* 死区外最小修正量，避免低于静摩擦的小碎步 */
#define YAW_DEADBAND_DEG10             12     /* 死区：1.2° 以内认为到位，不输出修正 */
#define YAW_REACQUIRE_DEG10            20     /* 重捕获阈值：到位后误差需超过 2.0° 才重新修正 */
#define YAW_REACQUIRE_CONFIRM_COUNT    2U     /* 重捕获确认次数：连续 2 周期超阈值才退出锁存 */

/* 积分分离参数 */
#define YAW_INTEGRAL_ZONE_DEG10        120    /* 积分区：12° 以内才允许积分积累 */
#define YAW_INTEGRAL_LIMIT_RPM         0      /* 积分限幅：保持态默认不用积分追尾差（Ki 极小） */

/* 小误差恢复曲线参数 */
#define YAW_MIN_TURN_ZONE_DEG10        180    /* 恢复区：18° 以内启用连续恢复速度曲线 */
#define YAW_TARGET_RAMP_STEP_DEG10     150    /* 目标斜坡步长：每 50ms 最多改变 15°（15°/s） */
#define YAW_RECOVER_MAX_TURN_RPM       15     /* 恢复曲线最大转向速度 */
#define YAW_APPROACH_BRAKE_ZONE_DEG10  120    /* 接近制动区：12° 以内快速靠近则清零制动 */
#define YAW_APPROACH_DERR_DEG10        2      /* 接近判定阈值：误差变化 ≥0.2°/周期 认为"快速靠近" */


/**
 * @brief  将 value 限幅到 [min_value, max_value] 区间。
 * @param  value      输入值
 * @param  min_value  下限
 * @param  max_value  上限（须 >= min_value）
 * @return 限幅后的值
 */
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

/**
 * @brief  将角度归一化到 [-1800, 1800]（即 ±180°，单位 0.1°）。
 *         通过加减 3600（即 360°）将任意角度折叠到目标范围。
 * @param  angle_deg10  输入角度 (×10)
 * @return 归一化后的角度
 */
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

/**
 * @brief  浮点角度（度）转为 deg10 整数（四舍五入）。
 *         正值+0.5、负值-0.5 确保截断方向正确。
 * @param  angle_deg  浮点角度（度）
 * @return deg10 整数
 */
int32_t yaw_float_deg_to_deg10(float angle_deg)
{
    if (angle_deg >= 0.0f) {
        return (int32_t)(angle_deg * 10.0f + 0.5f);
    }
    return (int32_t)(angle_deg * 10.0f - 0.5f);
}

/**
 * @brief  浮点角度归一化到 (-180, 180]。
 *         通过加减 360° 将任意角度折叠到目标范围。
 * @param  angle  输入角度（度）
 * @return 归一化后的角度
 */
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

/**
 * @brief  目标斜坡：每周期向 target 靠近 step_deg10，取最短角差方向。
 *         避免目标发生阶跃跳变（如用户从 0° 突设 180°），让小车平缓转向。
 *
 * @param  current_deg10  当前斜坡输出 (deg10)
 * @param  target_deg10   最终目标 (deg10)
 * @param  step_deg10     每周期最大变化量 (deg10)。负值取绝对值。
 * @return 斜坡后的新目标 (deg10)，已归一化
 */
static int32_t yaw_target_ramp_step(int32_t current_deg10,
                                    int32_t target_deg10,
                                    int32_t step_deg10)
{
    int32_t delta = yaw_normalize_deg10(target_deg10 - current_deg10);

    if (step_deg10 < 0) {
        step_deg10 = -step_deg10;
    }
    /* 如果步长为 0 或在一步范围内，直接跳到最终目标 */
    if (step_deg10 == 0 || util_abs_i32(delta) <= step_deg10) {
        return yaw_normalize_deg10(target_deg10);
    }

    /* 沿最短方向靠近 */
    if (delta > 0) {
        current_deg10 += step_deg10;
    } else {
        current_deg10 -= step_deg10;
    }
    return yaw_normalize_deg10(current_deg10);
}

/**
 * @brief  小误差恢复曲线：在死区外、zone 内，从 min_turn 到 max_turn 线性插值。
 *         目的是让误差很小时（刚出死区）给小转向，误差稍大时给大转向，
 *         避免低于静摩擦力的无效小 PWM。
 *
 * @param  abs_err_deg10   误差绝对值 (deg10)
 * @param  deadband_deg10  死区阈值 (deg10)，低于此值不输出
 * @param  zone_deg10      恢复区范围 (deg10)，超出此范围直接给 max_turn
 * @param  min_turn_rpm    最小转向量 (RPM)，zone 边界处输出此值
 * @return 建议转向量 (RPM)。死区内或 min_turn<=0 返回 0。
 */
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

    /* 线性插值：deadband 处输出 min_turn，zone 处输出 max_turn */
    int32_t span = zone_deg10 - deadband_deg10;
    int32_t pos = abs_err_deg10 - deadband_deg10;
    return min_turn_rpm + ((max_turn - min_turn_rpm) * pos) / span;
}


void yaw_control_init(yaw_control_t *control)
{
    if (control == 0) {
        return;
    }
    /* 初始化位置式 PID，装入默认 Kp/Ki/Kd 和输出限幅 */
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


/**
 * @brief  yaw PID 计算主体（约 90 行核心逻辑）。
 *
 *         集成了以下策略：
 *         1. 死区判断：误差在 1.2° 内不输出（pid_reset）
 *         2. 接近检测：误差方向与微分方向相反且变化足够快，标记 approaching_fast
 *         3. 积分分离：12° 内才积分，否则衰减一半；接近制动时额外衰减一半
 *         4. 动态限幅：限幅值 = 18 + 误差/14，但不低于最小修正
 *         5. 接近制动：快速靠近且在 12° 内时清零输出
 *         6. 静态补偿恢复曲线：输出过小时，用恢复曲线替代 PID 输出
 *
 * @param  pid                  位置式 PID 实例指针
 * @param  err_deg10            当前误差 (deg10)
 * @param  allow_static_boost   是否允许静态补偿（仅在斜坡已到达最终目标时允许）
 * @param  derr_out             [out] 本周期误差变化量 (deg10/周期)
 * @param  approaching_close_out [out] true=接近目标且误差在制动区内
 * @return 转向量 turn_rpm (RPM)
 */
static int32_t yaw_pid_compute_turn(pid_pos_t *pid,
                                    int32_t err_deg10,
                                    bool allow_static_boost,
                                    int32_t *derr_out,
                                    bool *approaching_close_out)
{
    /* 清零输出参数 */
    if (derr_out != 0) {
        *derr_out = 0;
    }
    if (approaching_close_out != 0) {
        *approaching_close_out = false;
    }
    if (pid == 0) {
        return 0;
    }

    int32_t abs_err = util_abs_i32(err_deg10);
    int32_t deadband = YAW_DEADBAND_DEG10;
    int32_t min_turn = YAW_MIN_TURN_RPM;
    int32_t izone = YAW_INTEGRAL_ZONE_DEG10;
    int32_t ilimit = YAW_INTEGRAL_LIMIT_RPM;

    /* 安全保证：积分区不得小于死区，否则死区内残留积分会累积 */
    if (izone < deadband) izone = deadband;
    if (ilimit < 0) ilimit = -ilimit;

    /* ────── 死区内：复位 PID，不输出转向 ────── */
    if (abs_err <= deadband) {
        pid_pos_reset(pid);
        return 0;
    }

    /* ────── 计算误差变化 derr ────── */
    /* pid_pos_get_derr_start() 内部处理 first_run 标志——首次调用时
     * 不产生正确 derr（因无历史值），直接返回 0。 */
    int32_t derr = pid_pos_get_derr_start(pid, err_deg10);
    if (derr_out != 0) {
        *derr_out = derr;
    }

    /* ────── 判断是否正在接近目标 ────── */
    /* approaching_target = (误差>0 且误差在缩小) 或 (误差<0 且误差在缩小) */
    bool approaching_target =
        ((err_deg10 > 0 && derr < 0) || (err_deg10 < 0 && derr > 0));
    bool approaching_fast = approaching_target &&
        (util_abs_i32(derr) >= YAW_APPROACH_DERR_DEG10);
    if (approaching_close_out != 0) {
        *approaching_close_out = approaching_fast &&
            abs_err <= YAW_APPROACH_BRAKE_ZONE_DEG10;
    }

    /* ────── 积分分离与衰减管理 ────── */
    int32_t ki = pid_pos_get_ki_milli(pid);
    int32_t integral = pid_pos_get_integral_milli(pid);
    if (abs_err <= izone && ki != 0) {
        /* izone 内：正常积分积累，带限幅保护 */
        int64_t next_integral = (int64_t)integral +
                                (int64_t)ki * err_deg10;
        int32_t int_limit_milli = ilimit * 1000;
        if (next_integral > int_limit_milli) {
            pid_pos_set_integral_milli(pid, int_limit_milli);
        } else if (next_integral < -int_limit_milli) {
            pid_pos_set_integral_milli(pid, -int_limit_milli);
        } else {
            pid_pos_set_integral_milli(pid, (int32_t)next_integral);
        }
    } else {
        /* izone 外：积分衰减（除以 2），避免误差大时积分 Windup */
        pid_pos_set_integral_milli(pid, integral / 2);
    }
    /* 接近制动时额外衰减一半积分，减少超调 */
    if (approaching_fast && abs_err <= YAW_APPROACH_BRAKE_ZONE_DEG10) {
        int32_t decayed = pid_pos_get_integral_milli(pid);
        pid_pos_set_integral_milli(pid, decayed / 2);
    }

    /* ────── PID 核心计算：P + D + 限幅 ────── */
    /* 积分项已由本函数准备好传参，pid 内部不再重复积分 */
    int32_t output = pid_pos_compute_with_integral(pid,
                                                    err_deg10,
                                                    pid_pos_get_integral_milli(pid),
                                                    0);

    /* ────── 动态限幅 ────── */
    /* 误差越大允许越大的输出，公式：cap = base + abs_err / divisor
     * 目的是：大误差时快速响应，小误差时精细调整 */
    int32_t dynamic_limit = YAW_DYNAMIC_CAP_BASE_RPM +
                            (abs_err / YAW_DYNAMIC_CAP_ERR_DIV);
    if (dynamic_limit < min_turn) {
        dynamic_limit = min_turn;
    }
    if (dynamic_limit > pid_pos_get_out_max(pid)) {
        dynamic_limit = pid_pos_get_out_max(pid);
    }
    output = clamp_i32(output, -dynamic_limit, dynamic_limit);

    /* ────── 接近制动 ────── */
    /* 误差快速变小且在制动区内，直接清零输出，依靠速度环自身的
     * 惯性/短接制动来"滑入"死区，避免 PID 超调和震荡。 */
    if (approaching_fast && abs_err <= YAW_APPROACH_BRAKE_ZONE_DEG10) {
        output = 0;
    }

    /* ────── 静态补偿（恢复曲线） ────── */
    /* 当 PID 输出小于最小修正值时，电机因静摩擦力无法转动。
     * 此时用恢复曲线替代 PID 输出，给一次明确的最小修正量帮助启动。 */
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
        output_same_direction && util_abs_i32(output) < recover_turn) {
        output = (err_sign > 0) ? recover_turn : -recover_turn;
    }

    /* 最终输出限幅并写回 pid->output */
    output = clamp_i32(output, pid->out_min, pid->out_max);
    pid->output = output;
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

    /* ────── 输出结构体默认值（安全清零） ────── */
    output->turn_rpm = 0;
    output->control_error_deg10 = 0;
    output->derr_deg10 = 0;
    output->settled = false;
    output->speed_ff_enable = false;
    output->approaching_close = false;

    if (control == 0) {
        return;
    }

    /* ────── 目标斜坡 ────── */
    /* 首次调用时将内部目标初始化为当前角度，避免从 0° 跳变到真实角度
     * 导致小车突然旋转。之后每个周期向最终目标靠近 step 步长。 */
    if (!control->control_target_initialized) {
        control->control_target_deg10 = current_yaw_deg10;
        control->control_target_initialized = true;
    }
    control->control_target_deg10 = yaw_target_ramp_step(
        control->control_target_deg10,
        target_yaw_deg10,
        YAW_TARGET_RAMP_STEP_DEG10);

    /* ────── 计算控制误差 ────── */
    output->control_error_deg10 = yaw_normalize_deg10(
        control->control_target_deg10 - current_yaw_deg10);
    int32_t abs_control_error = util_abs_i32(output->control_error_deg10);

    /* ────── 到位锁存状态下的重捕获判定 ────── */
    /* 已经到位（锁存）后，需要连续 2 个周期误差超过 2.0° 才退出锁存
     * 重新启动 PID。这是为了避免 MPU 噪声或机械回弹导致的反复启停。 */
    if (control->settled_latch) {
        if (abs_control_error >= YAW_REACQUIRE_DEG10) {
            if (control->reacquire_count < YAW_REACQUIRE_CONFIRM_COUNT) {
                control->reacquire_count++;
            }
            if (control->reacquire_count >= YAW_REACQUIRE_CONFIRM_COUNT) {
                /* 连续确认：退出锁存，复位 PID */
                control->settled_latch = false;
                control->reacquire_count = 0;
                pid_pos_reset(&control->pid);
            }
        } else {
            control->reacquire_count = 0;   /* 误差回落：清零确认计数 */
        }
    }

    /* 锁存态：不输出转向，保持不动 */
    if (control->settled_latch) {
        pid_pos_reset(&control->pid);
        output->turn_rpm = 0;
        output->settled = true;
        output->approaching_close = true;
        return;
    }

    /* ────── 执行 PID 计算 ────── */
    /* allow_static_boost 仅在斜坡已到达最终目标时才允许启用，
     * 避免斜坡中途静态补偿干扰导致超调。 */
    bool allow_static_boost = (control->control_target_deg10 == target_yaw_deg10);
    output->turn_rpm = yaw_pid_compute_turn(&control->pid,
                                            output->control_error_deg10,
                                            allow_static_boost,
                                            &output->derr_deg10,
                                            &output->approaching_close);

    /* ────── 到位判定与锁存 ────── */
    /* 进入死区 或 接近制动完成 时锁存到位 */
    if (abs_control_error <= YAW_DEADBAND_DEG10 ||
        (output->turn_rpm == 0 && output->approaching_close &&
         abs_control_error < YAW_REACQUIRE_DEG10)) {
        control->settled_latch = true;
        control->reacquire_count = 0;
        pid_pos_reset(&control->pid);
        output->turn_rpm = 0;
    }

    output->settled = control->settled_latch;

    /* ────── 速度环低速前馈使能 ────── */
    /* 仅当以下条件全部满足时才允许速度环启用低速前馈：
     * - 尚未到位（未锁存）
     * - PID 有输出
     * - 误差在死区外
     * - 未进入接近制动状态 */
    output->speed_ff_enable = (!output->settled && output->turn_rpm != 0 &&
                               abs_control_error > YAW_DEADBAND_DEG10 &&
                               !output->approaching_close);
}
