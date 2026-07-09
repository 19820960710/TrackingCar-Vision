#ifndef YAW_LOOP_SERVICE_H
#define YAW_LOOP_SERVICE_H

#include "task/app_tasks.h"
#include <stdbool.h>
#include <stdint.h>

bool yaw_loop_service_init(void);
void yaw_loop_service_step_10ms(void);

bool yaw_loop_service_set_target(int32_t base_speed_rpm,
                                 int32_t target_yaw_deg10);
bool yaw_loop_service_get_status(app_yaw_status_t *out);
bool yaw_loop_service_is_settled(void);

#endif /* YAW_LOOP_SERVICE_H */
