/* ============================================================================
 *   闲鱼定制 小研分享屋
 *   任何非闲鱼小研分享屋出售的均为盗版
 *   正式比赛代码绑定机器绑定芯片，任何二手出售均无效
 *   请认准正版
 * ============================================================================ */

/**
 * @file    yaw_control.h
 * @brief   yaw 角环纯算法：目标斜坡、位置式 PID、动态限幅、到位锁存与重捕获滞回。
 *
 * @details 单位约定：角度使用 deg10（0.1°），PID 系数使用 milli（×1000）。
 *
 *          本模块为纯算法，不依赖 FreeRTOS/硬件驱动，由 yaw_loop_service 调用。
 *          所有状态保存在 yaw_control_t 结构体中，支持多实例（当前为单例使用）。
 *
 *          === 控制策略 ===
 *          - 目标斜坡（150 个 0.1°/50ms ≈ 15°/s）平滑过渡，避免阶跃冲击；
 *          - 动态限幅：误差越大允许越大的转向速度，公式 cap = 18 + err/14 (RPM)；
 *          - 死区外给明确最小修正（15 RPM），克服静摩擦；
 *          - 积分分离：12° 内积分，外衰减；接近制动时额外衰减；
 *          - 接近制动：快速靠近目标时清零输出，让速度环滑入；
 *          - 到位锁存 + 重捕获确认（连续 2 周期超 2.0° 才重新修正），
 *            过滤 MPU 噪声与机械回弹导致的反复进/出死区。
 *
 *          === 参数调优建议 ===
 *          - 小车转动惯量大 → 增大 Kd 抑制超调，或减小 Kp 降低响应速度；
 *          - 到位后震荡 → 增大死区 YAW_DEADBAND_DEG10 或增大重捕获阈值；
 *          - 转向响应慢 → 增大 Kp 或 YAW_DYNAMIC_CAP_BASE_RPM；
 *          - 小误差爬行 → 调整 YAW_MIN_TURN_RPM 和恢复曲线参数。
 */
#ifndef YAW_CONTROL_H
#define YAW_CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include "pid/pid.h"

/**
 * @brief yaw 控制器单次运行输出。
 *
 * app_tasks 胶水层读取这些输出字段后做差速换算和速度环下发，
 * yaw 算法模块本身不依赖外部调用方。*/
typedef struct {
    int32_t turn_rpm;              /**< yaw 输出差速修正量 (RPM) */
    int32_t control_error_deg10;   /**< 参与控制的误差 (deg10) */
    int32_t derr_deg10;            /**< 本周期误差变化量 (deg10/50ms) */
    bool settled;                  /**< true=已进入到位保持区 */
    bool speed_ff_enable;          /**< true=允许速度环启用低速前馈 */
    bool approaching_close;        /**< true=接近目标且误差快速变小（制动态） */
} yaw_control_output_t;

/**
 * @brief yaw 控制器运行时状态。
 *
 * 结构体成员由内部维护，外部只传指针调用 init/reset/update。
 * 可读成员（如 pid.output）在调试时可查看，但不建议外部修改。
 */
typedef struct {
    pid_pos_t pid;                       /**< 位置式 PID 实例（含 Kp/Ki/Kd/积分/输出） */
    int32_t control_target_deg10;        /**< 斜坡后的内部目标角 ×10 */
    bool control_target_initialized;     /**< 内部目标是否已初始化为当前角度 */
    bool settled_latch;                  /**< 到位锁存标志 */
    uint8_t reacquire_count;             /**< 重捕获确认计数（连续超阈值周期数） */
} yaw_control_t;

/**
 * @brief  角度归一化到 [-1800, 1800]（±180°，单位 0.1°）。
 * @param  angle_deg10  输入角度
 * @return 归一化后的角度
 */
int32_t yaw_normalize_deg10(int32_t angle_deg10);

/**
 * @brief  浮点角度（度）转 deg10 整数（四舍五入）。
 * @param  angle_deg  浮点角度
 * @return deg10 整数
 */
int32_t yaw_float_deg_to_deg10(float angle_deg);

/**
 * @brief  浮点角度归一化到 (-180, 180]。
 * @param  angle  浮点角度
 * @return 归一化后角度
 */
float yaw_normalize_deg(float angle);

/**
 * @brief  初始化 yaw 控制器：装入默认 PID 参数并复位全部状态。
 * @param  control  控制器指针（非空）
 */
void yaw_control_init(yaw_control_t *control);

/**
 * @brief  复位 yaw 控制器：清零 PID 积分/输出、内部目标与到位锁存。
 * @param  control  控制器指针（非空）
 * @note   通常在切换目标或 yaw 环 disable/enable 时调用。
 */
void yaw_control_reset(yaw_control_t *control);

/**
 * @brief  执行一次 yaw 角环计算（50ms 周期）。
 *
 *         内部依次执行：
 *         1. 目标斜坡（靠近最终目标一步）
 *         2. 到位锁存 → 重捕获判断
 *         3. PID 计算（含积分分离/动态限幅/接近制动/静态补偿）
 *         4. 到位判定与锁存
 *
 * @param  control          控制器指针
 * @param  target_yaw_deg10 最终目标偏航角 ×10
 * @param  current_yaw_deg10 当前实测偏航角 ×10
 * @param  output           [out] 输出结果（turn_rpm/到位/前馈标志等）
 */
void yaw_control_update(yaw_control_t *control,
                        int32_t target_yaw_deg10,
                        int32_t current_yaw_deg10,
                        yaw_control_output_t *output);

#endif /* YAW_CONTROL_H */
