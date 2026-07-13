#include "vision_uart.h"

#include "vision_config.h"
#include "vision_packet.h"

#include <stddef.h>
#include <string.h>

static volatile uint8_t g_rx_ring[VISION_UART_RX_RING_CAPACITY];
static volatile uint16_t g_rx_head;
static volatile uint16_t g_rx_tail;
static volatile uint32_t g_rx_overrun_count;

static char g_line[VISION_UART_LINE_MAX];
static uint8_t g_line_length;
static bool g_discard_until_newline;

static vision_uart_config_t g_config;
static vision_observation_t g_latest_observation;
static vision_uart_stats_t g_stats;
static bool g_observation_ready;
static uint32_t g_next_sequence;

static void restore_interrupt_state(uint32_t primask)
{
    if ((primask & 1U) == 0U) {
        __enable_irq();
    }
}

static uint16_t next_ring_index(uint16_t index)
{
    index++;
    return (index >= VISION_UART_RX_RING_CAPACITY) ? 0U : index;
}

static void push_rx_byte(uint8_t byte)
{
    uint16_t next_head = next_ring_index(g_rx_head);

    if (next_head == g_rx_tail) {
        g_rx_overrun_count++;
        return;
    }

    g_rx_ring[g_rx_head] = byte;
    g_rx_head = next_head;
}

static bool pop_rx_byte(uint8_t *byte)
{
    uint32_t primask;

    if (byte == NULL) {
        return false;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if (g_rx_tail == g_rx_head) {
        restore_interrupt_state(primask);
        return false;
    }

    *byte = g_rx_ring[g_rx_tail];
    g_rx_tail = next_ring_index(g_rx_tail);
    restore_interrupt_state(primask);
    return true;
}

static uint32_t take_rx_overrun_count(void)
{
    uint32_t count;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    count = g_rx_overrun_count;
    g_rx_overrun_count = 0U;
    restore_interrupt_state(primask);
    return count;
}

static bool publish_line(uint32_t now_ms)
{
    vision_packet_t packet;
    vision_observation_t observation;

    g_line[g_line_length] = '\0';
    if (!vision_packet_parse_line(g_line, &packet) ||
        !vision_packet_to_observation(&packet, g_config.frame_width,
                                      g_config.frame_height,
                                      g_next_sequence + 1U, now_ms,
                                      &observation)) {
        g_stats.parse_error_count++;
        return false;
    }

    g_next_sequence++;
    g_latest_observation = observation;
    g_observation_ready = true;
    g_stats.packet_count++;
    return true;
}

static bool process_rx_byte(uint8_t byte, uint32_t now_ms)
{
    if (byte == '\r') {
        return false;
    }

    if (g_discard_until_newline) {
        if (byte == '\n') {
            g_discard_until_newline = false;
            g_line_length = 0U;
        }
        return false;
    }

    if (byte == '\n') {
        bool published = publish_line(now_ms);

        g_line_length = 0U;
        return published;
    }

    if (g_line_length >= (VISION_UART_LINE_MAX - 1U)) {
        g_line_length = 0U;
        g_discard_until_newline = true;
        g_stats.overrun_count++;
        return false;
    }

    g_line[g_line_length] = (char) byte;
    g_line_length++;
    return false;
}

bool vision_uart_init(const vision_uart_config_t *config)
{
    if ((config == NULL) || (config->instance == NULL) ||
        (config->frame_width == 0U) || (config->frame_height == 0U)) {
        return false;
    }

    g_config = *config;
    g_rx_head = 0U;
    g_rx_tail = 0U;
    g_rx_overrun_count = 0U;
    g_line_length = 0U;
    g_discard_until_newline = false;
    g_observation_ready = false;
    g_next_sequence = 0U;
    memset(&g_latest_observation, 0, sizeof(g_latest_observation));
    memset(&g_stats, 0, sizeof(g_stats));

    NVIC_ClearPendingIRQ(g_config.irqn);
    NVIC_EnableIRQ(g_config.irqn);
    return true;
}

void vision_uart_on_uart_irq(void)
{
    if (g_config.instance == NULL) {
        return;
    }

    switch (DL_UART_Main_getPendingInterrupt(g_config.instance)) {
        case DL_UART_MAIN_IIDX_RX:
        case DL_UART_MAIN_IIDX_RX_TIMEOUT_ERROR:
            while (!DL_UART_Main_isRXFIFOEmpty(g_config.instance)) {
                push_rx_byte(DL_UART_Main_receiveData(g_config.instance));
            }
            break;
        default:
            break;
    }
}

bool vision_uart_process(uint32_t now_ms)
{
    uint8_t byte;
    uint32_t lost_bytes = take_rx_overrun_count();
    bool published = false;

    if (lost_bytes != 0U) {
        g_stats.overrun_count += lost_bytes;
        g_line_length = 0U;
        g_discard_until_newline = true;
    }

    while (pop_rx_byte(&byte)) {
        if (process_rx_byte(byte, now_ms)) {
            published = true;
        }
    }

    return published;
}

bool vision_uart_take_latest_observation(vision_observation_t *out)
{
    if ((out == NULL) || !g_observation_ready) {
        return false;
    }

    *out = g_latest_observation;
    g_observation_ready = false;
    return true;
}

void vision_uart_get_stats(vision_uart_stats_t *out)
{
    if (out != NULL) {
        *out = g_stats;
    }
}

void vision_uart_send_line(const char *line)
{
    if ((line == NULL) || (g_config.instance == NULL)) {
        return;
    }

    while (*line != '\0') {
        DL_UART_Main_transmitDataBlocking(g_config.instance, (uint8_t) *line);
        line++;
    }
    DL_UART_Main_transmitDataBlocking(g_config.instance, (uint8_t) '\n');
}
