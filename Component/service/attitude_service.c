#include "service/attitude_service.h"
#include "mpu6050/mpu6050.h"
#include "queue.h"
#include <stdint.h>

#define MPU_STABLE_REQUIRED_SAMPLES    80U
#define MPU_STABLE_PITCH_RANGE_DEG     1.50f
#define MPU_STABLE_ROLL_RANGE_DEG      1.50f
#define MPU_STABLE_YAW_RANGE_DEG       1.00f

typedef struct {
    float pitch;
    float roll;
    float yaw;
} attitude_raw_t;

static QueueHandle_t g_attitude_queue = NULL;
static TaskHandle_t g_attitude_task_handle = NULL;

static int32_t attitude_float_to_deg10(float angle_deg)
{
    if (angle_deg >= 0.0f) {
        return (int32_t)(angle_deg * 10.0f + 0.5f);
    }
    return (int32_t)(angle_deg * 10.0f - 0.5f);
}

static float attitude_normalize_deg(float angle)
{
    while (angle > 180.0f) {
        angle -= 360.0f;
    }
    while (angle <= -180.0f) {
        angle += 360.0f;
    }
    return angle;
}

static void attitude_publish_raw(const attitude_raw_t *raw, bool valid)
{
    if (g_attitude_queue == NULL) {
        return;
    }

    app_attitude_t msg = {0};
    if (raw != NULL) {
        msg.pitch_deg10 = attitude_float_to_deg10(raw->pitch);
        msg.roll_deg10 = attitude_float_to_deg10(raw->roll);
        msg.yaw_deg10 = attitude_float_to_deg10(raw->yaw);
    }
    msg.valid = valid;
    (void)xQueueOverwrite(g_attitude_queue, &msg);
}

bool attitude_service_init(void)
{
    if (g_attitude_queue == NULL) {
        g_attitude_queue = xQueueCreate(1, sizeof(app_attitude_t));
    }
    return (g_attitude_queue != NULL);
}

bool attitude_service_get(app_attitude_t *out)
{
    if (out == NULL || g_attitude_queue == NULL ||
        xQueuePeek(g_attitude_queue, out, 0) != pdPASS) {
        return false;
    }
    return true;
}

void attitude_service_notify_from_isr(BaseType_t *higher_priority_task_woken)
{
    if (g_attitude_task_handle != NULL) {
        vTaskNotifyGiveFromISR(g_attitude_task_handle,
                               higher_priority_task_woken);
    }
}

void attitude_service_task(void *arg)
{
    (void)arg;
    uint16_t stable_count = 0;
    float pitch_min = 0.0f;
    float pitch_max = 0.0f;
    float roll_min = 0.0f;
    float roll_max = 0.0f;
    float yaw_min = 0.0f;
    float yaw_max = 0.0f;
    float yaw_window_ref = 0.0f;
    float yaw_zero_offset = 0.0f;
    bool attitude_ready = false;

    g_attitude_task_handle = xTaskGetCurrentTaskHandle();

    vTaskDelay(pdMS_TO_TICKS(200));
    if (MPU6050_Init() != 0) {
        attitude_publish_raw(NULL, false);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    (void)ulTaskNotifyTake(pdTRUE, 0);

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (Read_Quad() != 0) {
            continue;
        }

        if (!attitude_ready) {
            if (stable_count == 0U) {
                pitch_min = pitch;
                pitch_max = pitch;
                roll_min = roll;
                roll_max = roll;
                yaw_window_ref = yaw;
                yaw_min = yaw;
                yaw_max = yaw;
                stable_count = 1U;
                continue;
            }

            float yaw_unwrapped = yaw_window_ref +
                attitude_normalize_deg(yaw - yaw_window_ref);

            if (pitch < pitch_min) pitch_min = pitch;
            if (pitch > pitch_max) pitch_max = pitch;
            if (roll < roll_min) roll_min = roll;
            if (roll > roll_max) roll_max = roll;
            if (yaw_unwrapped < yaw_min) yaw_min = yaw_unwrapped;
            if (yaw_unwrapped > yaw_max) yaw_max = yaw_unwrapped;

            stable_count++;
            if (stable_count < MPU_STABLE_REQUIRED_SAMPLES) {
                continue;
            }

            if (((pitch_max - pitch_min) <= MPU_STABLE_PITCH_RANGE_DEG) &&
                ((roll_max - roll_min) <= MPU_STABLE_ROLL_RANGE_DEG) &&
                ((yaw_max - yaw_min) <= MPU_STABLE_YAW_RANGE_DEG)) {
                yaw_zero_offset = yaw;
                attitude_ready = true;
            } else {
                stable_count = 0U;
                continue;
            }
        }

        attitude_raw_t raw = {
            .pitch = pitch,
            .roll = roll,
            .yaw = attitude_normalize_deg(yaw - yaw_zero_offset),
        };
        attitude_publish_raw(&raw, true);
    }
}
