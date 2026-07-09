#ifndef ATTITUDE_SERVICE_H
#define ATTITUDE_SERVICE_H

#include "FreeRTOS.h"
#include "task/app_tasks.h"
#include <stdbool.h>

bool attitude_service_init(void);
void attitude_service_task(void *arg);
bool attitude_service_get(app_attitude_t *out);
void attitude_service_notify_from_isr(BaseType_t *higher_priority_task_woken);

#endif /* ATTITUDE_SERVICE_H */
