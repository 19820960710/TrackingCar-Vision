/**
 * @file    speed_loop_core.h
 * @brief   速度环纯算法核心：编码器 delta→RPM 转换、一阶低通滤波、
 *          目标斜坡、增量式 PID、低速前馈与 PWM 输出计算。
 *
 * @details 本模块不依赖 FreeRTOS、编码器驱动或 TB6612，仅做纯算法运算。
 *          所有状态封装在 speed_loop_core_t 结构体中，支持多实例。
 *          硬件适配由 speed_service 负责：读取编码器增量传入，取输出 PWM 写入电机驱动。
 *
 *          === 节拍约定 ===
 *          - 采样周期：10ms（由 TIMER_0 硬件中断驱动）
 *          - 控制周期：50ms（5 次采样累计一个窗口做 PID）
 *          这样的好处是 10ms 高频采样可以捕获编码器脉冲细节，
 *          而 50ms 窗口累计可以减少量化噪声对 PID 的影响。
 *
 *          === 控制策略 ===
 *          - 增量式 PID：天然抗积分饱和，只输出变化量
 *          - 目标斜坡：每 50ms 最多变化 100 RPM，减小冲击
 *          - 一阶低通滤波：系数 0.5，滤除编码器量化噪声
 *          - 低速前馈：克服静摩擦，带比例衰减和方向检查
 *
 *          === 参数调优建议 ===
 *          - 电机响应慢 → 增大 Kp 或 Ki
 *          - PWM 抖动/噪声 → 减小 Kd（当前已为 0），或减小 Kp
 *          - 低速启动困难 → 调整 SPEED_START_FF 参数组
 *          - 超调/过冲 → 减小 Ki 或增大目标斜坡步长
 */
#ifndef SPEED_LOOP_CORE_H
#define SPEED_LOOP_CORE_H

#include "pid/pid.h"
#include <stdbool.h>
#include <stdint.h>

/** 速度环控制周期 50ms（5 个 10ms 采样窗口）。 */
#define SPEED_LOOP_CORE_CONTROL_PERIOD_MS 50U
/** 速度环采样周期 10ms（由 TIMER_0 硬件节拍驱动）。 */
#define SPEED_LOOP_CORE_SAMPLE_PERIOD_MS  10U

/**
 * @brief  速度环核心运行时状态。
 *
 *         结构体成员由核心内部维护，外部仅通过 getter 或 update 的输出参数读取。
 *         所有字段均为私有；修改请通过 API，不要直接写入。
 */
typedef struct {
    pid_inc_t left_pid;          /**< 左轮增量式 PID 实例 */
    pid_inc_t right_pid;         /**< 右轮增量式 PID 实例 */

    int32_t left_rpm10_filt;     /**< 左轮 RPM×10 一阶低通滤波值（保留 1 位小数精度） */
    int32_t right_rpm10_filt;    /**< 右轮 RPM×10 一阶低通滤波值 */

    int32_t left_target_rpm;     /**< 左轮目标速度 (RPM)，由外部设定 */
    int32_t right_target_rpm;    /**< 右轮目标速度 (RPM)，由外部设定 */
    int32_t left_setpoint_rpm;   /**< 左轮斜坡后的实际设定点 (RPM) */
    int32_t right_setpoint_rpm;  /**< 右轮斜坡后的实际设定点 (RPM) */

    int32_t left_delta_sum;      /**< 左轮编码器增量累计（当前 50ms 窗口内） */
    int32_t right_delta_sum;     /**< 右轮编码器增量累计（当前 50ms 窗口内） */
    uint32_t sample_count;       /**< 当前窗口已采样次数（满 5 次执行 PID） */

    int32_t left_rpm;            /**< 左轮实测速度 (RPM)，滤波后取整 */
    int32_t right_rpm;           /**< 右轮实测速度 (RPM)，滤波后取整 */

    int32_t left_pwm;            /**< 左轮 PWM 输出值 */
    int32_t right_pwm;           /**< 右轮 PWM 输出值 */
} speed_loop_core_t;

/**
 * @brief  速度环核心输出快照。
 *
 *         brake=true 时应调用 tb6612_brake()（电机短接制动），
 *         而非 tb6612_set_speed()（PWM 驱动）。
 *         这是因为 0 PWM 时电机仍会惯性转动，短接绕组可更快停止。
 */
typedef struct {
    int32_t left_rpm;            /**< 左轮实测 RPM */
    int32_t right_rpm;           /**< 右轮实测 RPM */
    int32_t left_target_rpm;     /**< 左轮目标 RPM */
    int32_t right_target_rpm;    /**< 右轮目标 RPM */
    int32_t left_pwm;            /**< 左轮 PWM 输出 */
    int32_t right_pwm;           /**< 右轮 PWM 输出 */
    bool stopped;                /**< true=已完全停稳（目标/设定点/PWM 均归零） */
    bool brake;                  /**< true=应执行电机短接制动 */
} speed_loop_core_output_t;

/**
 * @brief  初始化速度环核心：清零全部状态，左右轮 PID 装入默认参数。
 * @param  ctx  核心上下文指针（非空）
 */
void speed_loop_core_init(speed_loop_core_t *ctx);

/**
 * @brief  设置左右轮目标速度。
 *
 * @param  ctx       核心上下文指针
 * @param  left_rpm  左轮目标 RPM
 * @param  right_rpm 右轮目标 RPM
 * @return true=目标均为 0 且已执行立即停止（调用方应据此调用制动）；
 *         false=正常设定
 */
bool speed_loop_core_set_target(speed_loop_core_t *ctx,
                                int32_t left_rpm,
                                int32_t right_rpm);

/**
 * @brief  10ms 采样入口：累计编码器增量，满 50ms 窗口执行 PID 并输出 PWM。
 *
 * @param  ctx              核心上下文指针
 * @param  left_delta       左轮编码器增量（本 10ms 内的脉冲数）
 * @param  right_delta      右轮编码器增量
 * @param  sample_period_ms 采样周期 (ms)，通常传 SPEED_LOOP_CORE_SAMPLE_PERIOD_MS
 * @param  counts_per_rev   编码器每转脉冲数（4 倍频后）
 * @param  start_ff_enable  是否允许低速前馈（由 yaw 控制层决定）
 * @param  out              [out] 输出快照（可为 NULL）
 * @return true=本周期完成了 PID 计算（满 50ms 窗口），调用方应输出 PWM 到电机；
 *         false=仍在累计窗口，无需输出
 */
bool speed_loop_core_update(speed_loop_core_t *ctx,
                            int32_t left_delta,
                            int32_t right_delta,
                            uint32_t sample_period_ms,
                            uint32_t counts_per_rev,
                            bool start_ff_enable,
                            speed_loop_core_output_t *out);

/**
 * @brief  只读取出当前输出快照（不执行计算）。
 * @param  ctx  核心上下文指针
 * @param  out  [out] 输出快照
 */
void speed_loop_core_get_output(const speed_loop_core_t *ctx,
                                speed_loop_core_output_t *out);

/**
 * @brief  便捷查询两轮是否已完全停稳。
 * @param  ctx  核心上下文指针
 * @return true=目标/设定点/PWM 均归零
 */
bool speed_loop_core_is_stopped(const speed_loop_core_t *ctx);

#endif /* SPEED_LOOP_CORE_H */
