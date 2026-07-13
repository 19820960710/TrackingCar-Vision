# MSPM0 Vision UART Module

This folder contains the reusable MSPM0G3507-side UART receiver for MaixCAM Pro vision output.

It is intentionally small and decoupled:

- `vision_uart.c/.h`: UART0 interrupt receive, line buffering, latest-packet cache.
- `vision_packet.c/.h`: Parser for MaixCAM `AIM,...` text packets.

## UART Wiring

```text
MaixCAM Pro A19 TX -> MSPM0G3507 PA11 UART0_RX
MaixCAM Pro A18 RX <- MSPM0G3507 PA10 UART0_TX
MaixCAM Pro GND    -> MSPM0G3507 GND
```

Use a shared ground. Do not power MaixCAM Pro from the MSPM0G3507 3.3 V rail.

## SysConfig Requirements

Configure UART0 in TI SysConfig:

```text
UART instance: UART0
Baud rate:     115200
TX pin:        PA10
RX pin:        PA11
Flow control:  None
Interrupts:    RX, RX_TIMEOUT_ERROR
RX FIFO:       enabled, one-entry threshold
```

The generated project must provide these symbols through `ti_msp_dl_config.h`:

```c
UART_0_INST
UART_0_INST_INT_IRQN
UART_0_INST_IRQHandler
```

## Minimal Integration

Add these files to the Keil Source group:

```text
vision_uart.c
vision_uart.h
vision_packet.c
vision_packet.h
```

In `main.c`:

```c
#include "ti_msp_dl_config.h"
#include "vision_uart.h"

int main(void)
{
    SYSCFG_DL_init();
    vision_uart_init();

    while (1) {
        vision_uart_process();
    }
}

void UART_0_INST_IRQHandler(void)
{
    vision_uart_on_uart_irq();
}
```

Application code can read the latest packet without knowing the CSV protocol:

```c
vision_packet_t packet;

if (vision_uart_get_latest_packet(&packet)) {
    if (packet.has_target) {
        /* packet.target_x, packet.target_y */
    }

    if (packet.aim_valid) {
        /* packet.aim_dx, packet.aim_dy */
    }
}
```

## Parsed Packet

The parser expects MaixCAM aim-mode lines:

```text
AIM,valid,dx,dy,target_x,target_y,laser_x,laser_y,target_mode,laser_color
```

Only the numeric fields are parsed on MSPM0. String fields are ignored so the protocol can add labels without breaking the controller.
