#ifndef ATTITUDE_SERVICE_H
#define ATTITUDE_SERVICE_H

#include "task/app_tasks.h"
#include <stdbool.h>

bool attitude_service_init(void);
bool attitude_service_begin(void);
void attitude_service_process_sample(void);
bool attitude_service_get(app_attitude_t *out);

#endif /* ATTITUDE_SERVICE_H */
