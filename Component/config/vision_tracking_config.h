/**
 * @file vision_tracking_config.h
 * @brief Tunable MaixCAM-to-gimbal tracking parameters.
 */
#ifndef VISION_TRACKING_CONFIG_H
#define VISION_TRACKING_CONFIG_H

#define VISION_TRACKING_ENABLED                 1U
#define VISION_TRACKING_USE_SIMULATED_INPUT      0U
#define VISION_TRACKING_TASK_PERIOD_MS          1U
#define VISION_TRACKING_COMMAND_PERIOD_MS      25U
#define VISION_TRACKING_TELEMETRY_PERIOD_MS    20U
#define VISION_TRACKING_DEADBAND_PIXELS         4
#define VISION_TRACKING_YAW_KP_PULSES_PER_PIXEL 0.28f
#define VISION_TRACKING_YAW_KD_PULSE_SECONDS_PER_PIXEL 0.000f
#define VISION_TRACKING_PITCH_KP_PULSES_PER_PIXEL 0.40f
#define VISION_TRACKING_PITCH_KD_PULSE_SECONDS_PER_PIXEL 0.005f
#define VISION_TRACKING_YAW_MAX_PULSES          400
#define VISION_TRACKING_PITCH_MAX_PULSES        400
#define VISION_TRACKING_SPEED_RPM               60U
#define VISION_TRACKING_ACCELERATION            20U

/*
 * Synthetic input is for command-path and transport checks only. One finite
 * target is emitted per interval; it is not a model of camera feedback.
 */
#define VISION_TRACKING_STIMULUS_SEED            20260719U
#define VISION_TRACKING_STIMULUS_INTERVAL_MS     1500U
#define VISION_TRACKING_STIMULUS_MAX_ERROR_X_PIXELS 80
#define VISION_TRACKING_STIMULUS_MAX_ERROR_Y_PIXELS 60

/* Preserves the verified polarity from tracking_vision. */
#define VISION_TRACKING_YAW_POSITIVE_IS_CW     false
#define VISION_TRACKING_PITCH_POSITIVE_IS_CW    true

#endif /* VISION_TRACKING_CONFIG_H */
