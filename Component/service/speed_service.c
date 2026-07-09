#include "service/speed_service.h"
#include "control/speed_loop_core.h"
#include "encoder/encoder.h"
#include "tb6612/tb6612.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <stddef.h>

typedef struct {
    int32_t left_rpm;
    int32_t right_rpm;
    bool low_speed_ff_enable;
} speed_target_msg_t;

static QueueHandle_t g_speed_target_queue = NULL;
static QueueHandle_t g_speed_state_queue = NULL;
static speed_loop_core_t g_speed_core;
static bool g_low_speed_ff_enable = false;

static bool speed_service_receive_target(speed_target_msg_t *target)
{
    if (g_speed_target_queue == NULL || target == NULL) {
        return false;
    }
    return (xQueueReceive(g_speed_target_queue, target, 0) == pdPASS);
}

static void speed_service_publish_output(const speed_loop_core_output_t *output)
{
    if (g_speed_state_queue == NULL || output == NULL) {
        return;
    }

    speed_service_state_t state = {0};
    state.left_rpm = output->left_rpm;
    state.right_rpm = output->right_rpm;
    state.left_target_rpm = output->left_target_rpm;
    state.right_target_rpm = output->right_target_rpm;
    state.stopped = output->stopped;
    (void)xQueueOverwrite(g_speed_state_queue, &state);
}

static void speed_service_publish_snapshot(void)
{
    speed_loop_core_output_t output = {0};
    speed_loop_core_get_output(&g_speed_core, &output);
    speed_service_publish_output(&output);
}

static void speed_service_apply_motor_output(const speed_loop_core_output_t *output)
{
    if (output == NULL) {
        return;
    }

    if (output->brake) {
        tb6612_brake();
    } else {
        tb6612_set_speed((int16_t)output->right_pwm,
                         (int16_t)output->left_pwm);
    }
}

bool speed_service_init(void)
{
    if (g_speed_target_queue == NULL) {
        g_speed_target_queue = xQueueCreate(1, sizeof(speed_target_msg_t));
    }
    if (g_speed_state_queue == NULL) {
        g_speed_state_queue = xQueueCreate(1, sizeof(speed_service_state_t));
    }
    if (g_speed_target_queue == NULL || g_speed_state_queue == NULL) {
        return false;
    }

    g_low_speed_ff_enable = false;
    speed_loop_core_init(&g_speed_core);
    encoder_reset();
    speed_service_publish_snapshot();
    return true;
}

bool speed_service_set_target_with_ff(int32_t left_rpm,
                                      int32_t right_rpm,
                                      bool low_speed_ff_enable)
{
    speed_target_msg_t target = {0};

    if (g_speed_target_queue == NULL) {
        return false;
    }
    target.left_rpm = left_rpm;
    target.right_rpm = right_rpm;
    target.low_speed_ff_enable = low_speed_ff_enable;
    return (xQueueOverwrite(g_speed_target_queue, &target) == pdPASS);
}

bool speed_service_set_target(int32_t left_rpm, int32_t right_rpm)
{
    return speed_service_set_target_with_ff(left_rpm, right_rpm, false);
}

void speed_service_step_10ms(void)
{
    speed_target_msg_t new_target;
    speed_loop_core_output_t output = {0};

    if (speed_service_receive_target(&new_target)) {
        g_low_speed_ff_enable = new_target.low_speed_ff_enable;
        if (speed_loop_core_set_target(&g_speed_core,
                                       new_target.left_rpm,
                                       new_target.right_rpm)) {
            tb6612_brake();
            speed_service_publish_snapshot();
        }
    }

    encoder_data_t encoder;
    encoder_get_data(&encoder);

    if (speed_loop_core_update(&g_speed_core,
                               encoder.left_delta,
                               encoder.right_delta,
                               SPEED_LOOP_CORE_SAMPLE_PERIOD_MS,
                               ENCODER_COUNTS_PER_REV,
                               g_low_speed_ff_enable,
                               &output)) {
        speed_service_apply_motor_output(&output);
    }

    speed_service_publish_snapshot();
}

bool speed_service_get_state(speed_service_state_t *out)
{
    if (g_speed_state_queue == NULL || out == NULL ||
        xQueuePeek(g_speed_state_queue, out, 0) != pdPASS) {
        return false;
    }
    return true;
}

bool speed_service_wheels_stopped_snapshot(void)
{
    speed_service_state_t state = {0};
    if (!speed_service_get_state(&state)) {
        return false;
    }
    return state.stopped;
}
