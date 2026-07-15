#ifndef GIMBAL_TRACKING_CONFIG_H
#define GIMBAL_TRACKING_CONFIG_H

#include "zdt_x42s.h"

/* Feature switches for commissioning and field operation. */
#define PITCH_MOTOR_COMMISSIONING_TEST_ENABLED (1U)
#define YAW_TRACKING_ENABLED                   (1U)
#define PITCH_TRACKING_ENABLED                 (1U)
#define GIMBAL_TRACKING_COMMISSIONING_MODE     (1U)
#define GIMBAL_PD_TUNING_ENABLED               (0U)
#define MOTOR_DIAGNOSTIC_ENABLED               (0U)

/* General application timing. */
#define LED_HEARTBEAT_PERIOD_MS                (500U)
#define MOTOR_DIAGNOSTIC_PERIOD_MS             (500U)

/* Axis polarity and tracker commissioning parameters. */
#define GIMBAL_YAW_POSITIVE_ERROR_IS_CW         (false)
#define GIMBAL_PITCH_POSITIVE_ERROR_IS_CW       (true)
#define GIMBAL_COMMISSIONING_DEADBAND_PIXELS    (4)
#define GIMBAL_YAW_KP_PULSES_PER_PIXEL          (0.40f)
#define GIMBAL_YAW_KD_PULSE_SECONDS_PER_PIXEL   (0.0f)
#define GIMBAL_PITCH_KP_PULSES_PER_PIXEL        (0.40f)
#define GIMBAL_PITCH_KD_PULSE_SECONDS_PER_PIXEL (0.005f)
#define GIMBAL_YAW_COMMISSIONING_MAXIMUM_PULSES   (400)
#define GIMBAL_PITCH_COMMISSIONING_MAXIMUM_PULSES (400)
#define GIMBAL_CONTROL_PERIOD_MS                  (25U)

/* Target-loss prediction and search scan parameters. */
#define TARGET_LOSS_PREDICTION_HOLD_MS        (300U)
#define TARGET_SEARCH_SCAN_PERIOD_MS          (40U)
#define TARGET_SEARCH_YAW_HALF_SPAN_DEGREES   (90U)
#define TARGET_SEARCH_PITCH_HALF_SPAN_DEGREES (30U)
#define TARGET_SEARCH_YAW_HALF_SPAN_PULSES                              \
    ((int32_t)((ZDT_X42S_EMM_PULSES_PER_REVOLUTION *                    \
               TARGET_SEARCH_YAW_HALF_SPAN_DEGREES + 180U) / 360U))
#define TARGET_SEARCH_PITCH_HALF_SPAN_PULSES                            \
    ((int32_t)((ZDT_X42S_EMM_PULSES_PER_REVOLUTION *                    \
               TARGET_SEARCH_PITCH_HALF_SPAN_DEGREES + 180U) / 360U))
#define TARGET_SEARCH_YAW_STEP_PULSES     (24)
#define TARGET_SEARCH_PITCH_STEP_PULSES   (24)
#define TARGET_SEARCH_SPEED_RPM           (60U)
#define TARGET_SEARCH_ACCELERATION        (20U)

/* Optional PD tuning sequence parameters. */
#define PD_TUNING_CENTER_STABLE_MS          (800U)
#define PD_TUNING_SETTLED_MS                (500U)
#define PD_TUNING_COOLDOWN_MS               (800U)
#define PD_TUNING_PERTURB_PERIOD_MS         (100U)
#define PD_TUNING_PERTURB_TIMEOUT_MS       (3000U)
#define PD_TUNING_RESPONSE_TIMEOUT_MS      (8000U)
#define PD_TUNING_PERTURB_STEP_PULSES         (8)
#define PD_TUNING_PERTURB_SPEED_RPM          (20U)
#define PD_TUNING_PERTURB_ACCELERATION       (10U)
#define PD_TUNING_YAW_TARGET_ERROR_PIXELS     (60)
#define PD_TUNING_PITCH_TARGET_ERROR_PIXELS   (40)
#define PD_TUNING_TRIAL_COUNT                  (4U)

#endif /* GIMBAL_TRACKING_CONFIG_H */
