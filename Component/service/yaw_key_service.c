#include "service/yaw_key_service.h"
#include "service/yaw_loop_service.h"
#include "led/key.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>
#include <stdbool.h>

void yaw_key_service_task(void *arg)
{
    (void)arg;
    bool key_was = false;
    int32_t yaw_deg10 = 0;

    for (;;) {
        bool key_now = key_read_user();

        if (key_now && !key_was) {
            int32_t base_speed_rpm = 0;
            app_yaw_status_t state = {0};

            yaw_deg10 += 450;
            if (yaw_deg10 > 1800) {
                yaw_deg10 = 0;
            }

            if (yaw_loop_service_get_status(&state)) {
                base_speed_rpm = state.base_speed_rpm;
            }
            (void)yaw_loop_service_set_target(base_speed_rpm, yaw_deg10);
        }

        key_was = key_now;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
