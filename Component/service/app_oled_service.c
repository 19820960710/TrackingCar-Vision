#include "service/app_oled_service.h"
#include "service/attitude_service.h"
#include "service/yaw_loop_service.h"
#include "task/app_tasks.h"
#include "oled/oled.h"
#include <stdint.h>
#include <stddef.h>

static int32_t abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static void oled_print_yaw_target(const app_yaw_status_t *yaw_state)
{
    int32_t tgt_abs = abs_i32(yaw_state->target_yaw_deg10);
    char tgt_sign = (yaw_state->target_yaw_deg10 < 0) ? '-' : ' ';

    OLED_vsprint(0, 32, 16, "YT:%c%3ld.%1ld %s", tgt_sign,
                 (long)(tgt_abs / 10), (long)(tgt_abs % 10),
                 yaw_state->enabled ? "ON " : "OFF");
}

void app_oled_service_init(app_oled_service_t *ctx)
{
    if (ctx != NULL) {
        ctx->attitude_seen = false;
        ctx->clear_count = 99;
    }

    OLED_Init();
    OLED_Clear();
    OLED_vsprint(0, 0, 16, "mpu init...");
    OLED_Refresh();
}

void app_oled_service_update(app_oled_service_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    app_attitude_t attitude = {0};
    app_yaw_status_t yaw_state = {0};

    if (attitude_service_get(&attitude)) {
        ctx->attitude_seen = true;
    }
    (void)yaw_loop_service_get_status(&yaw_state);

    ctx->clear_count++;
    if (ctx->clear_count >= 100) {
        OLED_Clear();
        ctx->clear_count = 0;
    }

    if (!ctx->attitude_seen) {
        OLED_vsprint(0, 0, 16, "MPU stabilizing");
        OLED_vsprint(0, 16, 16, "wait about 20s ");
        oled_print_yaw_target(&yaw_state);
        OLED_vsprint(0, 48, 16, "yaw not ready  ");
    } else if (!attitude.valid) {
        OLED_vsprint(0, 0, 16, "mpu failure    ");
        OLED_vsprint(0, 16, 16, "yaw target: ---");
        OLED_vsprint(0, 32, 16, "yaw now   : ---");
        OLED_vsprint(0, 48, 16, "check MPU6050  ");
    } else {
        int32_t tgt_abs = abs_i32(yaw_state.target_yaw_deg10);
        int32_t now_abs = abs_i32(attitude.yaw_deg10);
        int32_t err_abs = abs_i32(yaw_state.error_yaw_deg10);
        char tgt_sign = (yaw_state.target_yaw_deg10 < 0) ? '-' : ' ';
        char now_sign = (attitude.yaw_deg10 < 0) ? '-' : ' ';
        char err_sign = (yaw_state.error_yaw_deg10 < 0) ? '-' : ' ';

        OLED_vsprint(0, 0, 16, "YT:%c%3ld.%1ld %s", tgt_sign,
                     (long)(tgt_abs / 10), (long)(tgt_abs % 10),
                     yaw_state.enabled ? "ON " : "OFF");
        OLED_vsprint(0, 16, 16, "YN:%c%3ld.%1ld", now_sign,
                     (long)(now_abs / 10), (long)(now_abs % 10));
        OLED_vsprint(0, 32, 16, "YE:%c%3ld.%1ld", err_sign,
                     (long)(err_abs / 10), (long)(err_abs % 10));
        OLED_vsprint(0, 48, 16, "B:%4ld T:%4ld",
                     (long)yaw_state.base_speed_rpm,
                     (long)yaw_state.turn_rpm);
    }

    OLED_Refresh();
}
