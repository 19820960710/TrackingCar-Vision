/**
 * @file    pid.h
 * @brief   增量式 PID 控制器接口定义 (千倍整数运算, 无浮点)
 *
 * ── 设计说明 ──
 * 1. 千倍整数: 所有系数 × 1000 存储, 保留 3 位小数精度, 避免浮点运算
 * 2. 增量式: 输出 = 上次输出 + 增量 Δu, 天然抗积分饱和
 * 3. 输出限幅: 在累积值上做 clamp, 等效于积分分离
 * 4. 独立控制器: 每个控制轴 (左轮/右轮) 需要独立的 pid_inc_t 实例
 *
 * ── 使用方式 ──
 * @code
 *   pid_inc_t motor_pid;
 *   // Kp=0.08, Ki=0.05, Kd=0, 输出范围 ±50
 *   pid_inc_init(&motor_pid, 80, 50, 0, -50, 50);
 *
 *   // 每个控制周期:
 *   int32_t output = pid_inc_compute(&motor_pid, target_rpm, actual_rpm);
 *   tb6612_set_speed(output, output);
 *
 *   // 目标变更时复位:
 *   pid_inc_reset(&motor_pid);
 * @endcode
 */
#ifndef PID_H
#define PID_H

#include <stdint.h>

/**
 * @brief 增量式 PID 控制器结构体
 *
 * 成员说明:
 *   kp_milli/ki_milli/kd_milli: 系数 × 1000 (例: 80 = Kp 0.080)
 *   out_min/out_max: 输出限幅
 *   output: 当前输出值 (外部读取)
 *   output_milli: 输出 × 1000 (内部精度保持)
 *   err_1: 上一次误差 e[k-1]
 *   err_2: 上上次误差 e[k-2]
 */
typedef struct {
    int32_t kp_milli;       /**< 比例系数 × 1000 */
    int32_t ki_milli;       /**< 积分系数 × 1000 */
    int32_t kd_milli;       /**< 微分系数 × 1000 */
    int32_t out_min;        /**< 输出下限 */
    int32_t out_max;        /**< 输出上限 */
    int32_t output;         /**< 当前输出值 (限幅后) */
    int32_t output_milli;   /**< 输出 × 1000 (内部高精度) */
    int32_t err_1;          /**< 上周期误差 e[k-1] */
    int32_t err_2;          /**< 上上周期误差 e[k-2] */
} pid_inc_t;

/**
 * @brief  初始化增量式 PID 控制器并复位状态
 * @param  pid       控制器指针
 * @param  kp_milli  比例系数 × 1000 (如 80 = 0.080)
 * @param  ki_milli  积分系数 × 1000
 * @param  kd_milli  微分系数 × 1000
 * @param  out_min   输出下限
 * @param  out_max   输出上限
 */
void pid_inc_init(pid_inc_t *pid, int32_t kp_milli, int32_t ki_milli,
                  int32_t kd_milli, int32_t out_min, int32_t out_max);

/**
 * @brief  在线调整 PID 系数 (不改变内部状态)
 * @param  pid       控制器指针
 * @param  kp_milli  新比例系数 × 1000
 * @param  ki_milli  新积分系数 × 1000
 * @param  kd_milli  新微分系数 × 1000
 */
void pid_inc_set_gain(pid_inc_t *pid, int32_t kp_milli, int32_t ki_milli,
                      int32_t kd_milli);

/**
 * @brief  在线调整输出限幅并约束当前输出
 * @param  pid      控制器指针
 * @param  out_min  新输出下限
 * @param  out_max  新输出上限
 */
void pid_inc_set_output_limit(pid_inc_t *pid, int32_t out_min, int32_t out_max);

/**
 * @brief  复位 PID 控制器 (输出清零, 清除历史误差)
 * @param  pid  控制器指针
 */
void pid_inc_reset(pid_inc_t *pid);

/**
 * @brief  执行一次 PID 计算
 * @param  pid       控制器指针
 * @param  target    目标值 (设定点)
 * @param  measured  实测值 (反馈值)
 * @return 限幅后的输出值
 */
int32_t pid_inc_compute(pid_inc_t *pid, int32_t target, int32_t measured);

/**
 * @brief  获取当前输出值 (不执行计算)
 * @param  pid  控制器指针
 * @return 当前输出值
 */
int32_t pid_inc_get_output(const pid_inc_t *pid);

#endif /* PID_H */
