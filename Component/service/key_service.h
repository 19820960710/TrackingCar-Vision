#ifndef YAW_KEY_SERVICE_H
#define YAW_KEY_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool key_was;
    int32_t yaw_deg10;
} yaw_key_service_t;

void yaw_key_service_init(yaw_key_service_t *ctx);
void yaw_key_service_step_10ms(yaw_key_service_t *ctx);

#endif /* YAW_KEY_SERVICE_H */
