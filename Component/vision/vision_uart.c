/**
 * @file vision_uart.c
 * @brief Interrupt-to-task UART3 adapter for MaixCAM text observations.
 */
#include "vision/vision_uart.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "ti_msp_dl_config.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define VISION_RX_QUEUE_LENGTH      192U
#define VISION_LINE_MAX              96U
#define VISION_TX_LINE_MAX            96U
#define VISION_FRAME_WIDTH          320U
#define VISION_FRAME_HEIGHT         240U

static QueueHandle_t g_rx_queue;
static char g_line[VISION_LINE_MAX];
static uint8_t g_line_length;
static bool g_discard_until_newline;
static vision_observation_t g_latest;
static bool g_latest_ready;
static vision_uart_stats_t g_stats;
static char g_tx_line[VISION_TX_LINE_MAX];
static uint8_t g_tx_length;
static uint8_t g_tx_index;

static bool parse_i32(const char **cursor, int32_t *out)
{
    const char *p = *cursor;
    uint32_t magnitude = 0U;
    bool negative = false;
    bool has_digit = false;
    uint32_t limit = (uint32_t)INT32_MAX;

    if (*p == '-') {
        negative = true;
        limit++;
        p++;
    }
    while ((*p >= '0') && (*p <= '9')) {
        uint32_t digit = (uint32_t)(*p - '0');
        if (magnitude > ((limit - digit) / 10U)) {
            return false;
        }
        magnitude = magnitude * 10U + digit;
        has_digit = true;
        p++;
    }
    if (!has_digit) {
        return false;
    }
    *out = negative ? ((magnitude == limit) ? INT32_MIN : -(int32_t)magnitude) :
                      (int32_t)magnitude;
    *cursor = p;
    return true;
}

static bool parse_separator_and_i32(const char **cursor, int32_t *out)
{
    if (**cursor != ',') {
        return false;
    }
    (*cursor)++;
    return parse_i32(cursor, out);
}

static bool token_equals(const char *token, size_t token_length,
                         const char *expected)
{
    size_t expected_length = strlen(expected);

    return (token_length == expected_length) &&
           (memcmp(token, expected, token_length) == 0);
}

static bool parse_aim_target_valid(const char **cursor, bool *target_valid)
{
    const char *token;
    size_t token_length;

    if ((**cursor != ',') || (target_valid == NULL)) {
        return false;
    }
    (*cursor)++;
    token = *cursor;
    while ((**cursor != '\0') && (**cursor != ',')) {
        (*cursor)++;
    }
    token_length = (size_t)(*cursor - token);
    if (token_length == 0U) {
        return false;
    }
    *target_valid = !token_equals(token, token_length, "NO_TARGET") &&
                    !token_equals(token, token_length, "LOST");
    return true;
}

static bool parse_target_line(const char *line, vision_observation_t *out)
{
    const char *cursor = line;
    int32_t field[7] = {0};
    uint8_t field_count;
    bool is_aim;

    if ((cursor[0] == 'A') && (cursor[1] == 'I') && (cursor[2] == 'M') &&
        (cursor[3] == ',')) {
        cursor += 4;
        field_count = 7U;
        is_aim = true;
    } else if ((cursor[0] == 'T') && (cursor[1] == 'V') && (cursor[2] == ',')) {
        cursor += 3;
        field_count = 5U;
        is_aim = false;
    } else {
        return false;
    }
    if (!parse_i32(&cursor, &field[0])) {
        return false;
    }
    for (uint8_t index = 1U; index < field_count; index++) {
        if (!parse_separator_and_i32(&cursor, &field[index])) {
            return false;
        }
    }
    if ((field[0] < 0) || (field[0] > 1) || (field[3] < 0) ||
        (field[4] < 0) || (field[3] >= VISION_FRAME_WIDTH) ||
        (field[4] >= VISION_FRAME_HEIGHT)) {
        return false;
    }

    if (is_aim) {
        /* AIM field[0] describes aim/laser validity, not target validity. */
        if (!parse_aim_target_valid(&cursor, &out->target_valid)) {
            return false;
        }
    } else {
        out->target_valid = field[0] == 1;
    }
    out->target_x = (uint16_t)field[3];
    out->target_y = (uint16_t)field[4];
    out->frame_width = VISION_FRAME_WIDTH;
    out->frame_height = VISION_FRAME_HEIGHT;
    return true;
}

static void publish_line(uint32_t now_ms)
{
    vision_observation_t observation;

    g_line[g_line_length] = '\0';
    if (!parse_target_line(g_line, &observation)) {
        g_stats.parse_error_count++;
        return;
    }
    observation.received_at_ms = now_ms;
    observation.sequence = g_latest.sequence + 1U;
    g_latest = observation;
    g_latest_ready = true;
    g_stats.valid_packet_count++;
}

bool vision_uart_init(void)
{
    uint8_t discarded;

    g_rx_queue = xQueueCreate(VISION_RX_QUEUE_LENGTH, sizeof(uint8_t));
    if (g_rx_queue == NULL) {
        return false;
    }
    while (DL_UART_receiveDataCheck(UART_VISION_INST, &discarded)) {
    }
    g_line_length = 0U;
    g_discard_until_newline = false;
    g_latest_ready = false;
    g_latest.sequence = 0U;
    g_stats = (vision_uart_stats_t){0};
    g_tx_length = 0U;
    g_tx_index = 0U;
    NVIC_SetPriority(UART_VISION_INST_INT_IRQN,
                     configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    DL_UART_enableInterrupt(UART_VISION_INST, DL_UART_INTERRUPT_RX);
    NVIC_EnableIRQ(UART_VISION_INST_INT_IRQN);
    return true;
}

void vision_uart_irq_handler(void)
{
    BaseType_t task_woken = pdFALSE;
    uint8_t byte;

    if (DL_UART_getPendingInterrupt(UART_VISION_INST) == DL_UART_IIDX_RX) {
        while (DL_UART_receiveDataCheck(UART_VISION_INST, &byte)) {
            if ((g_rx_queue == NULL) ||
                (xQueueSendFromISR(g_rx_queue, &byte, &task_woken) != pdPASS)) {
                g_stats.rx_overrun_count++;
            }
        }
    }
    portYIELD_FROM_ISR(task_woken);
}

void vision_uart_process(uint32_t now_ms)
{
    uint8_t byte;

    while ((g_rx_queue != NULL) &&
           (xQueueReceive(g_rx_queue, &byte, 0U) == pdPASS)) {
        if (byte == '\r') {
            continue;
        }
        if (g_discard_until_newline) {
            if (byte == '\n') {
                g_discard_until_newline = false;
                g_line_length = 0U;
            }
            continue;
        }
        if (byte == '\n') {
            publish_line(now_ms);
            g_line_length = 0U;
        } else if (g_line_length < (VISION_LINE_MAX - 1U)) {
            g_line[g_line_length++] = (char)byte;
        } else {
            g_line_length = 0U;
            g_discard_until_newline = true;
            g_stats.rx_overrun_count++;
        }
    }
}

bool vision_uart_take_latest(vision_observation_t *out)
{
    if ((out == NULL) || !g_latest_ready) {
        return false;
    }
    *out = g_latest;
    g_latest_ready = false;
    return true;
}

void vision_uart_get_stats(vision_uart_stats_t *out)
{
    if (out != NULL) {
        *out = g_stats;
    }
}

bool vision_uart_send_line(const char *line)
{
    uint8_t length = 0U;

    if ((line == NULL) || (g_tx_length != 0U)) {
        return false;
    }
    while ((line[length] != '\0') && (length < (VISION_TX_LINE_MAX - 2U))) {
        g_tx_line[length] = line[length];
        length++;
    }
    if (line[length] != '\0') {
        return false;
    }
    g_tx_line[length++] = '\n';
    g_tx_length = length;
    g_tx_index = 0U;
    return true;
}

void vision_uart_service_tx(void)
{
    while ((g_tx_index < g_tx_length) &&
           !DL_UART_isTXFIFOFull(UART_VISION_INST)) {
        DL_UART_transmitData(UART_VISION_INST, (uint8_t)g_tx_line[g_tx_index]);
        g_tx_index++;
    }
    if (g_tx_index >= g_tx_length) {
        g_tx_length = 0U;
        g_tx_index = 0U;
    }
}
