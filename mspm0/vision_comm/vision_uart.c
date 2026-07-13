#include "vision_uart.h"

#include "ti_msp_dl_config.h"

#include <stddef.h>
#include <string.h>

#define VISION_UART_LINE_MAX (96U)

static volatile char g_rx_line[VISION_UART_LINE_MAX];
static volatile uint8_t g_rx_len;
static volatile bool g_line_ready;
static volatile bool g_line_overrun;

static vision_uart_data_t g_latest;

static void push_rx_byte(uint8_t byte)
{
    if (byte == '\r') {
        return;
    }

    if (g_line_ready) {
        g_line_overrun = true;
        return;
    }

    if (byte == '\n') {
        g_rx_line[g_rx_len] = '\0';
        g_line_ready = true;
        return;
    }

    if (g_rx_len >= (VISION_UART_LINE_MAX - 1U)) {
        g_rx_len = 0U;
        g_line_overrun = true;
        return;
    }

    g_rx_line[g_rx_len] = (char) byte;
    g_rx_len++;
}

void vision_uart_init(void)
{
    g_rx_len = 0U;
    g_line_ready = false;
    g_line_overrun = false;
    memset(&g_latest, 0, sizeof(g_latest));

    NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
}

void vision_uart_on_uart_irq(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_0_INST)) {
        case DL_UART_MAIN_IIDX_RX:
            while (!DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
                push_rx_byte(DL_UART_Main_receiveData(UART_0_INST));
            }
            break;
        case DL_UART_MAIN_IIDX_RX_TIMEOUT_ERROR:
            while (!DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
                push_rx_byte(DL_UART_Main_receiveData(UART_0_INST));
            }
            break;
        default:
            break;
    }
}

bool vision_uart_process(void)
{
    char line[VISION_UART_LINE_MAX];
    bool has_line;
    bool had_overrun;
    vision_packet_t parsed_packet;

    __disable_irq();
    has_line = g_line_ready;
    had_overrun = g_line_overrun;
    if (has_line) {
        for (uint8_t i = 0U; i < VISION_UART_LINE_MAX; i++) {
            line[i] = g_rx_line[i];
            if (line[i] == '\0') {
                break;
            }
        }
        g_rx_len = 0U;
        g_line_ready = false;
    }
    g_line_overrun = false;
    __enable_irq();

    if (had_overrun) {
        g_latest.parse_error_count++;
    }

    if (!has_line) {
        return false;
    }

    if (!vision_packet_parse_aim_line(line, &parsed_packet)) {
        g_latest.parse_error_count++;
        return false;
    }

    g_latest.packet = parsed_packet;
    g_latest.updated = true;
    g_latest.packet_count++;
    return true;
}

bool vision_uart_get_latest(vision_uart_data_t *out)
{
    bool updated;

    if (out == NULL) {
        return false;
    }

    __disable_irq();
    *out = g_latest;
    updated = g_latest.updated;
    __enable_irq();
    return updated;
}

bool vision_uart_get_latest_packet(vision_packet_t *out)
{
    bool updated;

    if (out == NULL) {
        return false;
    }

    __disable_irq();
    *out = g_latest.packet;
    updated = g_latest.updated;
    __enable_irq();
    return updated;
}

void vision_uart_clear_updated(void)
{
    __disable_irq();
    g_latest.updated = false;
    __enable_irq();
}

void vision_uart_send_line(const char *line)
{
    if (line == NULL) {
        return;
    }

    while (*line != '\0') {
        DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t) *line);
        line++;
    }
    DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t) '\n');
}
