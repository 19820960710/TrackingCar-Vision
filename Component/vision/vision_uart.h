/**
 * @file vision_uart.h
 * @brief MaixCAM UART3 receive driver and target-observation interface.
 */
#ifndef VISION_UART_H
#define VISION_UART_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool target_valid;
    uint16_t target_x;
    uint16_t target_y;
    uint16_t frame_width;
    uint16_t frame_height;
    uint32_t received_at_ms;
    uint32_t sequence;
} vision_observation_t;

typedef struct {
    uint32_t valid_packet_count;
    uint32_t parse_error_count;
    uint32_t rx_overrun_count;
} vision_uart_stats_t;

bool vision_uart_init(void);
void vision_uart_irq_handler(void);
void vision_uart_process(uint32_t now_ms);
bool vision_uart_take_latest(vision_observation_t *out);
void vision_uart_get_stats(vision_uart_stats_t *out);
bool vision_uart_send_line(const char *line);
void vision_uart_service_tx(void);

#endif /* VISION_UART_H */
