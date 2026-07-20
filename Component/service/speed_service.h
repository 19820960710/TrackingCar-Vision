/* ============================================================================
 *   闲鱼定制 小研分享屋
 *   任何非闲鱼小研分享屋出售的均为盗版
 *   正式比赛代码绑定机器绑定芯片，任何二手出售均无效
 *   请认准正版
 * ============================================================================ */

/**
 * @file speed_service.h
 * @brief 30 ms、mm/s 双轮速度环的 FreeRTOS 与硬件适配层。
 */
#ifndef SPEED_SERVICE_H
#define SPEED_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#define SPEED_SERVICE_SAMPLE_PERIOD_MS 10U

typedef struct {
    float left_speed_mm_s;
    float right_speed_mm_s;
    float left_target_mm_s;
    float right_target_mm_s;
    int32_t left_encoder_count;
    int32_t right_encoder_count;
    int32_t left_pwm_duty_count;
    int32_t right_pwm_duty_count;
    bool stopped;
} speed_service_state_t;

bool speed_service_init(void);
bool speed_service_set_target_mm_s(float left_mm_s, float right_mm_s);
void speed_service_step_10ms(void);
bool speed_service_get_state(speed_service_state_t *out);
bool speed_service_wheels_stopped_snapshot(void);

#endif /* SPEED_SERVICE_H */
