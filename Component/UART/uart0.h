#ifndef UART0_H
#define UART0_H

#include <stdint.h>

void uart0_init(void);
void uart0_sendStr(const char *str);

/* FreeRTOS 任务函数 */
void uart0_Send_task(void *arg);
void uart0_Recive_task(void *arg);

#endif
