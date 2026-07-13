#ifndef VISION_UART_H_
#define VISION_UART_H_

#include "vision_packet.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool updated;
    vision_packet_t packet;
    uint32_t packet_count;
    uint32_t parse_error_count;
} vision_uart_data_t;

void vision_uart_init(void);
void vision_uart_on_uart_irq(void);
bool vision_uart_process(void);
bool vision_uart_get_latest(vision_uart_data_t *out);
bool vision_uart_get_latest_packet(vision_packet_t *out);
void vision_uart_clear_updated(void);
void vision_uart_send_line(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* VISION_UART_H_ */
