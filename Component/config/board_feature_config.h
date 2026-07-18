/**
 * @file board_feature_config.h
 * @brief Optional board peripherals for functional validation.
 */
#ifndef BOARD_FEATURE_CONFIG_H
#define BOARD_FEATURE_CONFIG_H

/*
 * OLED uses software I2C. Keep it disabled during camera/gimbal validation so
 * an absent or stalled display cannot consume CPU time or block the test.
 */
#define BOARD_OLED_TASK_ENABLED    0U

#endif /* BOARD_FEATURE_CONFIG_H */
