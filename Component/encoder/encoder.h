#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

/* 实测：两轮手动转一圈均为 440 个计数。 */
#define ENCODER_COUNTS_PER_REV     (440)
#define ENCODER_LEFT_COUNTS_PER_REV  ENCODER_COUNTS_PER_REV
#define ENCODER_RIGHT_COUNTS_PER_REV ENCODER_COUNTS_PER_REV

/**
 * @brief 编码器数据快照。
 * left_count/right_count 为累计计数；left_delta/right_delta 为上次读取后的增量。
 */
typedef struct {
    int32_t left_count;
    int32_t right_count;
    int32_t left_delta;
    int32_t right_delta;
} encoder_data_t;

/**
 * @brief 初始化编码器读取。
 *        实测映射：物理右轮使用 TIMG8 硬件 QEI；物理左轮使用 PA25/PA14 GPIO 双边沿软件解码。
 */
void encoder_init(void);

/** @brief 清零累计计数。 */
void encoder_reset(void);

/** @brief 读取编码器累计计数和增量。 */
void encoder_get_data(encoder_data_t *data);

/** @brief GPIOA 编码器中断是否待处理。 */
int encoder_right_int_is_pending(void);

/** @brief 右轮 GPIO 软件解码中断处理，需在 GROUP1_IRQHandler 中调用。 */
void encoder_right_irq_handler(void);

#endif /* ENCODER_H */
