/**
 * @file stepper_service.c
 * @brief yaw/pitch 两个 ZDT X42S 的独立 UART FreeRTOS 服务层。
 */
#include "service/stepper_service.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "UART/stepper_uart.h"
#include "config/stepper_service_config.h"
#include "config/stepper_uart_config.h"

#include <stddef.h>

#define STEPPER_COMMAND_QUEUE_LENGTH 8U

typedef enum {
    STEPPER_COMMAND_ENABLE = 0,
    STEPPER_COMMAND_MOVE
} stepper_command_type_t;

typedef struct {
    stepper_command_type_t type;
    stepper_axis_t axis;
    bool enable;
    stepper_motor_move_t move;
} stepper_command_t;

static QueueHandle_t g_command_queue = NULL;
static QueueHandle_t g_state_queue[STEPPER_AXIS_COUNT] = {NULL};
static stepper_motor_t g_motor[STEPPER_AXIS_COUNT];
static stepper_service_state_t g_state[STEPPER_AXIS_COUNT];
static stepper_axis_t g_transport_axis[STEPPER_AXIS_COUNT] = {
    STEPPER_AXIS_YAW,
    STEPPER_AXIS_PITCH,
};
static bool g_available = false;

static const uint8_t g_address[STEPPER_AXIS_COUNT] = {
    ZDT_X42S_YAW_ADDRESS,
    ZDT_X42S_PITCH_ADDRESS,
};

static bool axis_is_valid(stepper_axis_t axis)
{
    return (uint32_t)axis < (uint32_t)STEPPER_AXIS_COUNT;
}

static stepper_uart_axis_t map_uart_axis(stepper_axis_t axis)
{
    return (axis == STEPPER_AXIS_PITCH) ? STEPPER_UART_PITCH :
                                          STEPPER_UART_YAW;
}

static bool uart_transport_write(void *context,
                                 const uint8_t *data,
                                 size_t length)
{
    const stepper_axis_t *axis = (const stepper_axis_t *)context;

    return (axis != NULL) && axis_is_valid(*axis) &&
           stepper_uart_write(map_uart_axis(*axis), data, length);
}

static bool uart_transport_read(void *context, uint8_t *byte)
{
    const stepper_axis_t *axis = (const stepper_axis_t *)context;

    return (axis != NULL) && axis_is_valid(*axis) &&
           stepper_uart_read_byte(map_uart_axis(*axis), byte, 0U);
}

static void publish_state(stepper_axis_t axis)
{
    if (axis_is_valid(axis) && (g_state_queue[axis] != NULL)) {
        (void)xQueueOverwrite(g_state_queue[axis], &g_state[axis]);
    }
}

bool stepper_service_init(void)
{
    stepper_service_state_t reset = {0};
    stepper_axis_t axis;

    if (g_command_queue == NULL) {
        g_command_queue = xQueueCreate(STEPPER_COMMAND_QUEUE_LENGTH,
                                       sizeof(stepper_command_t));
    }
    if (g_command_queue == NULL) {
        return false;
    }
    for (axis = STEPPER_AXIS_YAW; axis < STEPPER_AXIS_COUNT; axis++) {
        const zdt_x42s_transport_t transport = {
            .write = uart_transport_write,
            .read = uart_transport_read,
            .context = &g_transport_axis[axis],
        };

        if (g_state_queue[axis] == NULL) {
            g_state_queue[axis] =
                xQueueCreate(1, sizeof(stepper_service_state_t));
        }
        if ((g_state_queue[axis] == NULL) ||
            !stepper_motor_init(&g_motor[axis], &transport,
                                g_address[axis])) {
            return false;
        }
        g_state[axis] = reset;
        g_state[axis].last_tx_ok = true;
        stepper_uart_flush_rx(map_uart_axis(axis));
        publish_state(axis);
    }
    g_available = true;
    return true;
}

bool stepper_service_set_axis_enabled(stepper_axis_t axis, bool enabled)
{
    stepper_command_t command = {0};

    if (!g_available || !axis_is_valid(axis) || (g_command_queue == NULL)) {
        return false;
    }
    command.type = STEPPER_COMMAND_ENABLE;
    command.axis = axis;
    command.enable = enabled;
    return xQueueSend(g_command_queue, &command, 0U) == pdPASS;
}

bool stepper_service_move_axis(stepper_axis_t axis,
                               const stepper_motor_move_t *move)
{
    stepper_command_t command = {0};

    if (!g_available || !axis_is_valid(axis) ||
        (g_command_queue == NULL) || (move == NULL)) {
        return false;
    }
    command.type = STEPPER_COMMAND_MOVE;
    command.axis = axis;
    command.move = *move;
    return xQueueSend(g_command_queue, &command, 0U) == pdPASS;
}

bool stepper_service_get_axis_state(stepper_axis_t axis,
                                    stepper_service_state_t *out)
{
    return g_available && axis_is_valid(axis) && (out != NULL) &&
           (g_state_queue[axis] != NULL) &&
           (xQueuePeek(g_state_queue[axis], out, 0U) == pdPASS);
}

bool stepper_service_set_enabled(bool enabled)
{
    return stepper_service_set_axis_enabled(STEPPER_AXIS_YAW, enabled);
}

bool stepper_service_move(const stepper_motor_move_t *move)
{
    return stepper_service_move_axis(STEPPER_AXIS_YAW, move);
}

bool stepper_service_get_state(stepper_service_state_t *out)
{
    return stepper_service_get_axis_state(STEPPER_AXIS_YAW, out);
}

static void execute_command(const stepper_command_t *command)
{
    stepper_axis_t axis = command->axis;
    bool tx_ok;

    if (!axis_is_valid(axis)) {
        return;
    }
    if (command->type == STEPPER_COMMAND_ENABLE) {
        tx_ok = stepper_motor_set_enabled(&g_motor[axis], command->enable);
        if (tx_ok) {
            g_state[axis].enabled = command->enable;
        }
    } else {
        /* 新的位置指令必须等待它自己的到位响应，不复用上一次的状态。 */
        g_state[axis].last_response = ZDT_X42S_RESPONSE_NONE;
        tx_ok = g_state[axis].enabled &&
                stepper_motor_move(&g_motor[axis], &command->move);
    }
    g_state[axis].last_tx_ok = tx_ok;
    if (tx_ok) {
        g_state[axis].transmitted_commands++;
    }
    publish_state(axis);
}

static void poll_responses(void)
{
    stepper_axis_t axis;

    for (axis = STEPPER_AXIS_YAW; axis < STEPPER_AXIS_COUNT; axis++) {
        stepper_motor_response_t response = stepper_motor_poll(&g_motor[axis]);
        if (response != ZDT_X42S_RESPONSE_NONE) {
            g_state[axis].last_response = response;
            publish_state(axis);
        }
    }
}

void stepper_service_task(void *argument)
{
    stepper_command_t command;
    (void)argument;

    if (!g_available) {
        vTaskSuspend(NULL);
    }
    for (;;) {
        if (xQueueReceive(g_command_queue, &command,
                          pdMS_TO_TICKS(STEPPER_SERVICE_POLL_PERIOD_MS)) == pdPASS) {
            execute_command(&command);
        }
        poll_responses();
        {
            bool pending = uxQueueMessagesWaiting(g_command_queue) != 0U;
            stepper_axis_t axis;
            for (axis = STEPPER_AXIS_YAW; axis < STEPPER_AXIS_COUNT; axis++) {
                g_state[axis].command_pending = pending;
                publish_state(axis);
            }
        }
    }
}
