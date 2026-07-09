#ifndef ATTITUDE_SERVICE_H
#define ATTITUDE_SERVICE_H

#include "task/app_tasks.h"
#include <stdbool.h>

bool attitude_service_init(void);
void attitude_service_reset(void);
void attitude_service_publish_invalid(void);
void attitude_service_process_sample(float pitch_deg,
                                     float roll_deg,
                                     float yaw_deg);
bool attitude_service_get(app_attitude_t *out);

#endif /* ATTITUDE_SERVICE_H */
