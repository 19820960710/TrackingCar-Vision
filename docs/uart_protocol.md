# UART Protocol

`maixcam/main.py` can send vision results to the main controller through UART.

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

## Aim Mode

For self-aiming, use:

```python
UART_OUTPUT_MODE = "aim"
```

One line is sent every `UART_SEND_EVERY_N_FRAMES` frames:

```text
AIM,valid,dx,dy,target_x,target_y,laser_x,laser_y,target_mode,laser_color\n
```

Fields:

- `AIM`: fixed header
- `valid`: `1` only when both target and laser are valid
- `dx`: `target_x - laser_x`
- `dy`: `target_y - laser_y`
- `target_x`, `target_y`: target center in image coordinates
- `laser_x`, `laser_y`: laser point in image coordinates
- `target_mode`: `perspective`, `blob-fallback`, or `LOST`
- `laser_color`: `green`, `red`, `NO_LASER`, or `LOST`

Example:

```text
AIM,1,-12,8,244,168,256,160,perspective,green
AIM,0,0,0,244,168,0,0,perspective,NO_LASER
AIM,0,0,0,0,0,0,0,LOST,LOST
```

Controller meaning:

```text
dx < 0: laser is right of target, move correction left
dx > 0: laser is left of target, move correction right
dy < 0: laser is below target, move correction up
dy > 0: laser is above target, move correction down
```

Check motor direction on the real gimbal. If movement is reversed, flip the sign in the controller.

## Target Mode

For older target-only tests, use:

```python
UART_OUTPUT_MODE = "target"
```

Format:

```text
TV,valid,dx,dy,x,y,mode\n
```

Here `dx` and `dy` are target offset from image center, not laser aiming error.
