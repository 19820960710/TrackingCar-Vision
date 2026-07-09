#include "service/key_service.h"
#include "service/yaw_loop_service.h"
#include "led/key.h"
#include "task/app_tasks.h"
#include <stddef.h>

void yaw_key_service_init(yaw_key_service_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->key_was = false;
    ctx->yaw_deg10 = 0;
}

void yaw_key_service_step_10ms(yaw_key_service_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    bool key_now = key_read_user();

    if (key_now && !ctx->key_was) {
        int32_t base_speed_rpm = 0;
        app_yaw_status_t state = {0};

        ctx->yaw_deg10 += 450;
        if (ctx->yaw_deg10 > 1800) {
            ctx->yaw_deg10 = 0;
        }

        if (yaw_loop_service_get_status(&state)) {
            base_speed_rpm = state.base_speed_rpm;
        }
        (void)yaw_loop_service_set_target(base_speed_rpm, ctx->yaw_deg10);
    }

    ctx->key_was = key_now;
}
