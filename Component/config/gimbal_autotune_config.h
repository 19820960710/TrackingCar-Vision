/**
 * @file gimbal_autotune_config.h
 * @brief Bounded camera-closed-loop gimbal autotune trial configuration.
 */
#ifndef GIMBAL_AUTOTUNE_CONFIG_H
#define GIMBAL_AUTOTUNE_CONFIG_H

#define GIMBAL_AUTOTUNE_ENABLED                  0U
#define GIMBAL_AUTOTUNE_BOOT_DIAGNOSTIC_ENABLED  0U
#define GIMBAL_AUTOTUNE_SEED             20260719U
#define GIMBAL_AUTOTUNE_TRIAL_COUNT              8U
#define GIMBAL_AUTOTUNE_START_DELAY_MS        1500U
#define GIMBAL_AUTOTUNE_AXIS_ENABLE_DELAY_MS   200U
#define GIMBAL_AUTOTUNE_STOP_SETTLE_MS          200U
#define GIMBAL_AUTOTUNE_OFFSET_SETTLE_MS       100U
#define GIMBAL_AUTOTUNE_MOVE_SETTLE_MS        1500U
#define GIMBAL_AUTOTUNE_POLL_PERIOD_MS          25U
#define GIMBAL_AUTOTUNE_OBSERVATION_PERIOD_MS  250U
#define GIMBAL_AUTOTUNE_POSITION_QUERY_TIMEOUT_MS 200U
#define GIMBAL_AUTOTUNE_OFFSET_TIMEOUT_MS     4000U
#define GIMBAL_AUTOTUNE_TRACK_TIMEOUT_MS      5000U
#define GIMBAL_AUTOTUNE_TARGET_LOST_MS        1000U
#define GIMBAL_AUTOTUNE_STABLE_SAMPLES           8U

/* These are relative pulses, not absolute yaw/pitch coordinates. */
#define GIMBAL_AUTOTUNE_OFFSET_MIN_PULSES      120
#define GIMBAL_AUTOTUNE_OFFSET_MAX_YAW_PULSES  400
#define GIMBAL_AUTOTUNE_OFFSET_MAX_PITCH_PULSES 400
#define GIMBAL_AUTOTUNE_SPEED_RPM               60U
#define GIMBAL_AUTOTUNE_ACCELERATION            20U
#define GIMBAL_AUTOTUNE_BOOT_DIAGNOSTIC_PULSES  200

/* The X42S position query uses 65536 encoder counts per revolution. The
 * installed command subdivision uses 3200 pulses per revolution. */
#define GIMBAL_AUTOTUNE_COMMAND_PULSES_PER_REV  3200U
#define GIMBAL_AUTOTUNE_POSITION_COUNTS_PER_REV 65536U
#define GIMBAL_AUTOTUNE_MIN_POSITION_PERCENT      50U

/* Mailbox command limits. They prevent an invalid debugger write from
 * applying unbounded gains to the live control loop. */
#define GIMBAL_AUTOTUNE_YAW_KP_MAX              0.60f
#define GIMBAL_AUTOTUNE_YAW_KD_MAX              0.030f
#define GIMBAL_AUTOTUNE_PITCH_KP_MAX            1.00f
#define GIMBAL_AUTOTUNE_PITCH_KD_MAX            0.030f

#endif /* GIMBAL_AUTOTUNE_CONFIG_H */
