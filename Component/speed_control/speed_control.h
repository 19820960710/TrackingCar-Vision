#ifndef SPEED_CONTROL_H
#define SPEED_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#define SPEED_CONTROL_PERIOD_MS 50U

typedef struct {
    int32_t left_rpm;         /**< 左轮实测速度 (RPM) */
    int32_t right_rpm;        /**< 右轮实测速度 (RPM) */
    int32_t left_target_rpm;  /**< 左轮目标速度 (RPM) */
    int32_t right_target_rpm; /**< 右轮目标速度 (RPM) */
    bool stopped;             /**< true=目标、斜坡和输出均已停稳 */
} speed_control_state_t;

bool speed_control_init(void);
bool speed_control_set_target(int32_t left_rpm, int32_t right_rpm);
bool speed_control_set_target_with_ff(int32_t left_rpm,
                                      int32_t right_rpm,
                                      bool low_speed_ff_enable);
void speed_control_step_10ms(void);
bool speed_control_get_state(speed_control_state_t *out);
bool speed_control_wheels_stopped_snapshot(void);

#endif /* SPEED_CONTROL_H */
