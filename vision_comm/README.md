# MSPM0 Vision UART Module

This folder receives MaixCAM text output and publishes a protocol-independent
`vision_observation_t` for the gimbal tracking layer.

- `vision_config.h`: protocol constants and MaixCAM image defaults.
- `vision_packet.c/.h`: complete-line parser for `AIM,...` and `TV,...`.
- `vision_uart.c/.h`: board-bound UART interrupt receive and observation cache.
- `vision_observation.c/.h`: the only target-coordinate contract and freshness
  helper used by tracking.

## Module Boundary

The tracker must not parse UART text, use laser coordinates, or depend on the
current MaixCAM algorithm. It calls `vision_uart_take_latest_observation()` and
uses only `target_valid`, `target_x`, `target_y`, image size, sequence and time.

The parser accepts both formats emitted by the current MaixCAM code:

```text
TV,valid,dx,dy,x,y,mode
AIM,valid,dx,dy,target_x,target_y,laser_x,laser_y,target_mode,laser_color
```

For `AIM`, `valid` means both the target and laser are valid. The tracking
observation instead uses `target_mode` so a target remains usable even while the
laser is absent. A coordinate of zero is valid when the protocol says the target
is valid.

## UART Wiring

Use the board connector labelled **UART4**, which maps to MSPM0 UART3:

```text
MaixCAM Pro A19 TX -> board UART4 RX / MSPM0 PB3 (UART3_RX)
MaixCAM Pro A18 RX <- board UART4 TX / MSPM0 PB2 (UART3_TX)
MaixCAM Pro GND    -> board UART4 GND
```

Only connect TX, RX and GND. MaixCAM must use its own power supply; do not join
the UART4 5 V pin to MaixCAM power. In TI SysConfig configure this UART at
115200 baud, no flow control, RX and RX-timeout interrupts, and enabled RX FIFO.

`UART3` and `UART5` remain available for the yaw and pitch X42S drivers. The
board connector labelled UART2 shares MCU UART2 with UART3, so it is not a third
independent serial port.

## Minimal Integration

Add `vision_observation.c`, `vision_packet.c` and `vision_uart.c` to the same
Keil source group as the application before compiling.

The exact generated SysConfig names depend on the name assigned in the TI GUI.
Replace the two placeholders below with the UART3 instance and IRQ symbols from
your generated `ti_msp_dl_config.h`.

```c
#include "vision_config.h"
#include "vision_uart.h"

static const vision_uart_config_t g_maixcam_uart = {
    .instance = <UART3_INSTANCE_FROM_SYSCONFIG>,
    .irqn = <UART3_IRQ_FROM_SYSCONFIG>,
    .frame_width = VISION_DEFAULT_FRAME_WIDTH,
    .frame_height = VISION_DEFAULT_FRAME_HEIGHT,
};

int main(void)
{
    vision_observation_t last_observation = {0};

    SYSCFG_DL_init();
    (void) vision_uart_init(&g_maixcam_uart);

    while (1) {
        uint32_t now_ms = app_monotonic_ms();

        (void) vision_uart_process(now_ms);
        if (vision_uart_take_latest_observation(&last_observation)) {
            /* Keep this as the last observation used by the tracker. */
        }
        if (!vision_observation_is_fresh(&last_observation, now_ms, 100U)) {
            /* Future tracker holds motor position and clears its control output. */
        }
    }
}

void <UART3_IRQHandler_FROM_SYSCONFIG>(void)
{
    vision_uart_on_uart_irq();
}
```

`app_monotonic_ms()` is supplied by the application timer and must be monotonic.
The tracker will later reject old observations with a timeout; this module only
records their reception time.

## Current Receive Limit

The ISR writes bytes to a 256-byte ring buffer and the main loop assembles lines.
When the ring is full, newly received bytes are discarded, `overrun_count`
increments, and the parser discards data through the next newline to resynchronize.
The main loop retains the newest valid observation when it drains multiple frames;
this is the desired behavior for real-time tracking.

## Parser Tests

`tests/vision_packet_test.c` is a hardware-free parser test source. It is not a
Keil application source file and must not be added to the MCU target group.
