#ifndef UART0_H
#define UART0_H

#include <stdint.h>
#include <stdbool.h>

void uart0_init(void);
void uart0_sendStr(const char *str);
bool uart0_recvByte(uint8_t *byte, uint32_t timeout_ms);

/* FreeRTOS 任务函数 */
void uart0_Send_task(void *arg);
void uart0_Recive_task(void *arg);

#endif
