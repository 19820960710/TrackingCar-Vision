#ifndef TB6612_H
#define TB6612_H

#include <stdint.h>

/**
 * @brief 初始化 TB6612 驱动
 *        启动 PWM 计数，方向脚初始化为停止（滑行）状态。
 *        应在 SYSCFG_DL_init() 之后调用。
 */
void tb6612_init(void);

/**
 * @brief 设置左电机（电机A）速度
 * @param speed -100 ~ 100，正数前进，负数后退，0 停止滑行
 */
void tb6612_set_left_speed(int16_t speed);

/**
 * @brief 设置右电机（电机B）速度
 * @param speed -100 ~ 100，正数前进，负数后退，0 停止滑行
 */
void tb6612_set_right_speed(int16_t speed);

/**
 * @brief 同时设置两电机速度
 * @param left  左电机速度 -100 ~ 100
 * @param right 右电机速度 -100 ~ 100
 */
void tb6612_set_speed(int16_t left, int16_t right);

/**
 * @brief 两电机停止（滑行，非刹车）
 */
void tb6612_stop(void);

/**
 * @brief 两电机刹车（短接制动）
 *        AIN1=AIN2=1 短接 A 电机，BIN1=BIN2=1 短接 B 电机
 */
void tb6612_brake(void);

#endif /* TB6612_H */
