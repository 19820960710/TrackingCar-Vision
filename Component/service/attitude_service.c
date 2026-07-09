#include "service/attitude_service.h"
#include "mpu6050/mpu6050.h"
#include "FreeRTOS.h"
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

typedef struct {
    uint16_t stable_count;
    float pitch_min;
    float pitch_max;
    float roll_min;
    float roll_max;
    float yaw_min;
    float yaw_max;
    float yaw_window_ref;
    float yaw_zero_offset;
    bool ready;
} attitude_stabilizer_t;

static QueueHandle_t g_attitude_queue = NULL;
static attitude_stabilizer_t g_stabilizer;

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

static void attitude_stabilizer_reset(attitude_stabilizer_t *st)
{
    attitude_stabilizer_t init = {0};
    *st = init;
}

static bool attitude_stabilizer_accept(attitude_stabilizer_t *st)
{
    if (st->ready) {
        return true;
    }

    if (st->stable_count == 0U) {
        st->pitch_min = pitch;
        st->pitch_max = pitch;
        st->roll_min = roll;
        st->roll_max = roll;
        st->yaw_window_ref = yaw;
        st->yaw_min = yaw;
        st->yaw_max = yaw;
        st->stable_count = 1U;
        return false;
    }

    float yaw_unwrapped = st->yaw_window_ref +
        attitude_normalize_deg(yaw - st->yaw_window_ref);

    if (pitch < st->pitch_min) st->pitch_min = pitch;
    if (pitch > st->pitch_max) st->pitch_max = pitch;
    if (roll < st->roll_min) st->roll_min = roll;
    if (roll > st->roll_max) st->roll_max = roll;
    if (yaw_unwrapped < st->yaw_min) st->yaw_min = yaw_unwrapped;
    if (yaw_unwrapped > st->yaw_max) st->yaw_max = yaw_unwrapped;

    st->stable_count++;
    if (st->stable_count < MPU_STABLE_REQUIRED_SAMPLES) {
        return false;
    }

    if (((st->pitch_max - st->pitch_min) <= MPU_STABLE_PITCH_RANGE_DEG) &&
        ((st->roll_max - st->roll_min) <= MPU_STABLE_ROLL_RANGE_DEG) &&
        ((st->yaw_max - st->yaw_min) <= MPU_STABLE_YAW_RANGE_DEG)) {
        st->yaw_zero_offset = yaw;
        st->ready = true;
        return true;
    }

    attitude_stabilizer_reset(st);
    return false;
}

bool attitude_service_init(void)
{
    if (g_attitude_queue == NULL) {
        g_attitude_queue = xQueueCreate(1, sizeof(app_attitude_t));
    }
    attitude_stabilizer_reset(&g_stabilizer);
    return (g_attitude_queue != NULL);
}

bool attitude_service_begin(void)
{
    if (MPU6050_Init() != 0) {
        attitude_publish_raw(NULL, false);
        return false;
    }
    attitude_stabilizer_reset(&g_stabilizer);
    return true;
}

void attitude_service_process_sample(void)
{
    if (Read_Quad() != 0) {
        return;
    }

    if (!attitude_stabilizer_accept(&g_stabilizer)) {
        return;
    }

    attitude_raw_t raw = {
        .pitch = pitch,
        .roll = roll,
        .yaw = attitude_normalize_deg(yaw - g_stabilizer.yaw_zero_offset),
    };
    attitude_publish_raw(&raw, true);
}

bool attitude_service_get(app_attitude_t *out)
{
    if (out == NULL || g_attitude_queue == NULL ||
        xQueuePeek(g_attitude_queue, out, 0) != pdPASS) {
        return false;
    }
    return true;
}
