#ifndef GEAR_MOTOR_ANGLE_CONTROL_H
#define GEAR_MOTOR_ANGLE_CONTROL_H

#include <stdint.h>

typedef enum {
    GEAR_MOTOR_ANGLE_STATUS_IDLE = 0,
    GEAR_MOTOR_ANGLE_STATUS_RUNNING,
    GEAR_MOTOR_ANGLE_STATUS_COMPLETE
} gear_motor_angle_status_t;

/**
 * @brief 鍗曡疆瑙掑害澶栫幆閰嶇疆銆? *
 * @note kp 鐨勯噺绾蹭负 1/s锛涜搴︾粺涓€浣跨敤 rad锛岃閫熷害缁熶竴浣跨敤 rad/s銆? *       鏈ā鍧楄緭鍑洪€熷害鐩爣锛屼笉鐩存帴鎿嶄綔 PWM 鎴栫數鏈洪┍鍔ㄣ€? */
typedef struct {
    float kp;
    float wheel_radius_mm;
    float max_angular_speed_rad_s;
    float angle_tolerance_rad;
    float stop_speed_tolerance_rad_s;
    uint16_t settle_cycles;
} gear_motor_angle_config_t;

typedef struct {
    gear_motor_angle_config_t config;
    float start_angle_rad;
    float target_angle_rad;
    float current_angle_rad;
    float measured_angular_speed_rad_s;
    float error_rad;
    float target_angular_speed_rad_s;
    float target_linear_speed_mm_s;
    uint16_t settled_cycles;
    gear_motor_angle_status_t status;
    uint8_t initialized;
} gear_motor_angle_controller_t;

typedef struct {
    float target_angular_speed_rad_s;
    float target_linear_speed_mm_s;
    gear_motor_angle_status_t status;
} gear_motor_angle_output_t;

/** @return 閰嶇疆鏈夋晥杩斿洖 1锛屽惁鍒欒繑鍥?0銆?*/
uint8_t gear_motor_angle_control_init(
    gear_motor_angle_controller_t *controller,
    const gear_motor_angle_config_t *config);

/** @brief 娓呴櫎褰撳墠瑙掑害浠诲姟锛屼繚鐣欏垵濮嬪寲閰嶇疆銆?*/
void gear_motor_angle_control_reset(
    gear_motor_angle_controller_t *controller);

/**
 * @brief 浠庡綋鍓嶆湁绗﹀彿杞寮€濮嬩竴涓浉瀵硅浆瑙掍换鍔°€? *
 * @param current_angle_rad 褰撳墠绱杞锛屽崟浣?rad銆? * @param delta_angle_rad   鐩稿鐩爣瑙掑害锛涙鍊兼杞紝璐熷€煎弽杞€? * @return 宸插垵濮嬪寲骞舵垚鍔熷紑濮嬭繑鍥?1锛屽惁鍒欒繑鍥?0銆? */
uint8_t gear_motor_angle_control_start_relative(
    gear_motor_angle_controller_t *controller,
    float current_angle_rad,
    float delta_angle_rad);

/**
 * @brief 鎵ц涓€娆″崟杞搴﹂棴鐜洿鏂般€? *
 * @details 姣斾緥瑙掑害澶栫幆鐢熸垚鐩爣杞閫熷害锛屽苟鎸夎疆鍗婂緞鎹㈢畻涓虹洰鏍囩嚎閫熷害銆? *          瑙掑害杩涘叆瀹瑰樊鍚庤緭鍑洪浂閫熷害锛涘彧鏈夊疄娴嬭閫熷害涔熻繘鍏ュ宸苟杩炵画婊¤冻
 *          settle_cycles 娆★紝浠诲姟鎵嶈繘鍏?COMPLETE銆? */
void gear_motor_angle_control_step(
    gear_motor_angle_controller_t *controller,
    float current_angle_rad,
    float measured_angular_speed_rad_s,
    gear_motor_angle_output_t *output);

uint8_t gear_motor_angle_control_is_complete(
    const gear_motor_angle_controller_t *controller);

#endif /* GEAR_MOTOR_ANGLE_CONTROL_H */

