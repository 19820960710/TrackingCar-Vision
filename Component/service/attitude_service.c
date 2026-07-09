#include "service/attitude_service.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <stdint.h>

#define MPU_STABLE_REQUIRED_SAMPLES    80U
#define MPU_STABLE_PITCH_RANGE_DEG     1.50f
#define MPU_STABLE_ROLL_RANGE_DEG      1.50f
#define MPU_STABLE_YAW_RANGE_DEG       1.00f

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

static void attitude_publish(float pitch_deg,
                             float roll_deg,
                             float yaw_deg,
                             bool valid)
{
    if (g_attitude_queue == NULL) {
        return;
    }

    app_attitude_t msg = {0};
    msg.pitch_deg10 = attitude_float_to_deg10(pitch_deg);
    msg.roll_deg10 = attitude_float_to_deg10(roll_deg);
    msg.yaw_deg10 = attitude_float_to_deg10(yaw_deg);
    msg.valid = valid;
    (void)xQueueOverwrite(g_attitude_queue, &msg);
}

void attitude_service_reset(void)
{
    attitude_stabilizer_t init = {0};
    g_stabilizer = init;
}

static bool attitude_stabilizer_accept(float pitch_deg,
                                       float roll_deg,
                                       float yaw_deg)
{
    attitude_stabilizer_t *st = &g_stabilizer;

    if (st->ready) {
        return true;
    }

    if (st->stable_count == 0U) {
        st->pitch_min = pitch_deg;
        st->pitch_max = pitch_deg;
        st->roll_min = roll_deg;
        st->roll_max = roll_deg;
        st->yaw_window_ref = yaw_deg;
        st->yaw_min = yaw_deg;
        st->yaw_max = yaw_deg;
        st->stable_count = 1U;
        return false;
    }

    float yaw_unwrapped = st->yaw_window_ref +
        attitude_normalize_deg(yaw_deg - st->yaw_window_ref);

    if (pitch_deg < st->pitch_min) st->pitch_min = pitch_deg;
    if (pitch_deg > st->pitch_max) st->pitch_max = pitch_deg;
    if (roll_deg < st->roll_min) st->roll_min = roll_deg;
    if (roll_deg > st->roll_max) st->roll_max = roll_deg;
    if (yaw_unwrapped < st->yaw_min) st->yaw_min = yaw_unwrapped;
    if (yaw_unwrapped > st->yaw_max) st->yaw_max = yaw_unwrapped;

    st->stable_count++;
    if (st->stable_count < MPU_STABLE_REQUIRED_SAMPLES) {
        return false;
    }

    if (((st->pitch_max - st->pitch_min) <= MPU_STABLE_PITCH_RANGE_DEG) &&
        ((st->roll_max - st->roll_min) <= MPU_STABLE_ROLL_RANGE_DEG) &&
        ((st->yaw_max - st->yaw_min) <= MPU_STABLE_YAW_RANGE_DEG)) {
        st->yaw_zero_offset = yaw_deg;
        st->ready = true;
        return true;
    }

    attitude_service_reset();
    return false;
}

bool attitude_service_init(void)
{
    if (g_attitude_queue == NULL) {
        g_attitude_queue = xQueueCreate(1, sizeof(app_attitude_t));
    }
    attitude_service_reset();
    return (g_attitude_queue != NULL);
}

void attitude_service_publish_invalid(void)
{
    attitude_publish(0.0f, 0.0f, 0.0f, false);
}

void attitude_service_process_sample(float pitch_deg,
                                     float roll_deg,
                                     float yaw_deg)
{
    if (!attitude_stabilizer_accept(pitch_deg, roll_deg, yaw_deg)) {
        return;
    }

    attitude_publish(pitch_deg,
                     roll_deg,
                     attitude_normalize_deg(yaw_deg - g_stabilizer.yaw_zero_offset),
                     true);
}

bool attitude_service_get(app_attitude_t *out)
{
    if (out == NULL || g_attitude_queue == NULL ||
        xQueuePeek(g_attitude_queue, out, 0) != pdPASS) {
        return false;
    }
    return true;
}
