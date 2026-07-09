#ifndef APP_OLED_SERVICE_H
#define APP_OLED_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool attitude_seen;
    uint16_t clear_count;
} app_oled_service_t;

void app_oled_service_init(app_oled_service_t *ctx);
void app_oled_service_update(app_oled_service_t *ctx);

#endif /* APP_OLED_SERVICE_H */
