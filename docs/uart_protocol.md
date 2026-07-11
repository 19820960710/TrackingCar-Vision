# UART Protocol

`maixcam/main.py` can send the target offset to the main controller through UART.

UART output is disabled by default:

```python
ENABLE_UART_OUTPUT = False
```

Enable it after wiring the main controller:

```python
ENABLE_UART_OUTPUT = True
UART_PORT = "/dev/ttyS1"
UART_BAUDRATE = 115200
UART_TX_PIN = "A19"
UART_RX_PIN = "A18"
UART_TX_FUNC = "UART1_TX"
UART_RX_FUNC = "UART1_RX"
```

## Wiring

Use crossed UART wiring:

```text
MaixCam TX -> Main controller RX
MaixCam RX -> Main controller TX
MaixCam GND -> Main controller GND
```

The default output uses UART1 to avoid UART0 boot logs and MaixVision communication.

## Message Format

One line is sent every `UART_SEND_EVERY_N_FRAMES` frames:

```text
TV,valid,dx,dy,x,y,mode\n
```

Fields:

- `TV`: fixed header
- `valid`: `1` if target is valid, `0` if target is lost
- `dx`: target x offset from image center, right is positive
- `dy`: target y offset from image center, down is positive
- `x`: target x coordinate in the image
- `y`: target y coordinate in the image
- `mode`: target detection mode, such as `perspective` or `LOST`

Example:

```text
TV,1,-12,8,244,168,perspective
TV,0,0,0,0,0,LOST
```

For a controller, start with `dx` and `dy`:

```text
dx < 0: target is left of image center
dx > 0: target is right of image center
dy < 0: target is above image center
dy > 0: target is below image center
```
