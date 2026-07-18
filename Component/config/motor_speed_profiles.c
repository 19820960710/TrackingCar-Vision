/**
 * @file motor_speed_profiles.c
 * @brief MG680 已验证档案与小电机安全模板。
 */
#include "config/motor_speed_profiles.h"

#include <stddef.h>

#define MOTOR_PROFILE_PI 3.1415926f
const motor_speed_profile_t g_mg680_large_motor_profile = {
    .profile_name = "mg680_large_motor_wheel65",
    .motor_model = "MG680",
    .size_class = MOTOR_SIZE_CLASS_LARGE,
    .commissioned = true,
    .nominal_voltage_v = 12.0f,
    .wheel_diameter_mm = 65.0f,

    /* 11 periods/motor rev * AB x4 * 56:1 reduction = 2464 counts/wheel rev. */
    .gear_ratio_motor_to_output = 56.0f,
    .gear_ratio_verified = true,
    .encoder_counts_per_motor_rev = 44.0f,
    .output_counts_per_wheel_rev = {
        [MOTOR_PROFILE_SIDE_LEFT] = 2464.0f,
        [MOTOR_PROFILE_SIDE_RIGHT] = 2464.0f,
    },
    .output_counts_verified = true,
    .drive_direction_sign = {1, 1},

    .sample_period_ms = 10U,
    .control_period_ms = 30U,
    .speed_filter_alpha = 0.70f,
    .pid = {
        [MOTOR_PROFILE_SIDE_LEFT] = {
            .kp = 16.0f,
            .ki = 80.0f,
            .kd = 0.0f,
            .integral_limit = 800.0f,
        },
        [MOTOR_PROFILE_SIDE_RIGHT] = {
            .kp = 16.0f,
            .ki = 80.0f,
            .kd = 0.0f,
            .integral_limit = 800.0f,
        },
    },
    .direction_change_stop_mm_s = 20.0f,
    .validated_min_speed_mm_s = 250.0f,
    .validated_max_speed_mm_s = 550.0f,
    .pwm_logical_max = 4000,
};

const motor_speed_profile_t g_small_motor_template_profile = {
    .profile_name = "small_motor_uncommissioned",
    .motor_model = "TBD",
    .size_class = MOTOR_SIZE_CLASS_SMALL,
    .commissioned = false,
    .nominal_voltage_v = 0.0f,
    .wheel_diameter_mm = 0.0f,
    .gear_ratio_motor_to_output = 0.0f,
    .gear_ratio_verified = false,
    .encoder_counts_per_motor_rev = 0.0f,
    .output_counts_per_wheel_rev = {0.0f, 0.0f},
    .output_counts_verified = false,
    .drive_direction_sign = {0, 0},
    .sample_period_ms = 0U,
    .control_period_ms = 0U,
    .speed_filter_alpha = 0.0f,
    .pid = {{0.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 0.0f}},
    .direction_change_stop_mm_s = 0.0f,
    .validated_min_speed_mm_s = 0.0f,
    .validated_max_speed_mm_s = 0.0f,
    .pwm_logical_max = 0,
};

bool motor_speed_profile_is_usable(const motor_speed_profile_t *profile)
{
    if ((profile == NULL) || !profile->commissioned ||
        !profile->output_counts_verified ||
        (profile->wheel_diameter_mm <= 0.0f) ||
        (profile->output_counts_per_wheel_rev[MOTOR_PROFILE_SIDE_LEFT] <= 0.0f) ||
        (profile->output_counts_per_wheel_rev[MOTOR_PROFILE_SIDE_RIGHT] <= 0.0f) ||
        ((profile->drive_direction_sign[MOTOR_PROFILE_SIDE_LEFT] != 1) &&
         (profile->drive_direction_sign[MOTOR_PROFILE_SIDE_LEFT] != -1)) ||
        ((profile->drive_direction_sign[MOTOR_PROFILE_SIDE_RIGHT] != 1) &&
         (profile->drive_direction_sign[MOTOR_PROFILE_SIDE_RIGHT] != -1)) ||
        (profile->sample_period_ms == 0U) ||
        (profile->control_period_ms == 0U) ||
        ((profile->control_period_ms % profile->sample_period_ms) != 0U) ||
        (profile->pwm_logical_max <= 0)) {
        return false;
    }
    return true;
}

float motor_speed_profile_wheel_circumference_mm(
    const motor_speed_profile_t *profile)
{
    if ((profile == NULL) || (profile->wheel_diameter_mm <= 0.0f)) {
        return 0.0f;
    }
    return MOTOR_PROFILE_PI * profile->wheel_diameter_mm;
}

float motor_speed_profile_rpm_to_mm_s(const motor_speed_profile_t *profile,
                                      float rpm)
{
    return rpm * motor_speed_profile_wheel_circumference_mm(profile) / 60.0f;
}

float motor_speed_profile_mm_s_to_rpm(const motor_speed_profile_t *profile,
                                      float speed_mm_s)
{
    float circumference = motor_speed_profile_wheel_circumference_mm(profile);
    return (circumference > 0.0f) ? (speed_mm_s * 60.0f / circumference) : 0.0f;
}
