/**
 * @file stepper_service_config.h
 * @brief Timing configuration for the shared yaw/pitch stepper service.
 */
#ifndef STEPPER_SERVICE_CONFIG_H
#define STEPPER_SERVICE_CONFIG_H

/* Match the old bare-metal service cadence during vision PD validation. */
#define STEPPER_SERVICE_POLL_PERIOD_MS    1U

#endif /* STEPPER_SERVICE_CONFIG_H */
