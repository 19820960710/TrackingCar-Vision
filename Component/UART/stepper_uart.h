/**
 * @file stepper_uart.h
 * @brief yaw(UART2) 与 pitch(UART1) 的 FreeRTOS 安全字节流接口。
 */
#ifndef STEPPER_UART_H
#define STEPPER_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"

typedef enum {
    STEPPER_UART_YAW = 0,
    STEPPER_UART_PITCH,
    STEPPER_UART_COUNT
} stepper_uart_axis_t;

bool stepper_uart_init(void);
bool stepper_uart_write(stepper_uart_axis_t axis,
                        const uint8_t *data,
                        size_t length);
bool stepper_uart_read_byte(stepper_uart_axis_t axis,
                            uint8_t *byte,
                            TickType_t timeout_ticks);
void stepper_uart_flush_rx(stepper_uart_axis_t axis);

#endif /* STEPPER_UART_H */
