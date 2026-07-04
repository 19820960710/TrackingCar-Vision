/**
 * @file    key.h
 * @brief   按键检测模块接口 (PB21, 软件消抖)
 */
#ifndef KEY_H
#define KEY_H

#include <stdbool.h>

/** @brief 初始化按键消抖模块 */
void key_init(void);

/**
 * @brief  读取按键状态 (消抖后)
 * @return true=按下(低电平), false=释放(高电平)
 * @note   按键: PB21, 内部上拉, 按下低电平
 *         消抖: 连续两次读到相同电平才更新
 *         建议调用周期: 10ms
 */
bool key_read_user(void);

#endif /* KEY_H */
