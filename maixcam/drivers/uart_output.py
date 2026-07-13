try:
    from maix import pinmap, uart
except ImportError:
    from maix.peripheral import pinmap, uart

from settings import *
from app import runtime_state as state
from vision.geometry import frame_center

UART_DEV = None
UART_ERROR_PRINTED = False
def init_uart_output():
    global UART_DEV, UART_ERROR_PRINTED

    if not ENABLE_UART_OUTPUT:
        return None

    try:
        if UART_TX_PIN and UART_TX_FUNC:
            pinmap.set_pin_function(UART_TX_PIN, UART_TX_FUNC)
        if UART_RX_PIN and UART_RX_FUNC:
            pinmap.set_pin_function(UART_RX_PIN, UART_RX_FUNC)
        UART_DEV = uart.UART(UART_PORT, UART_BAUDRATE)
        print("uart output ready: %s %d" % (UART_PORT, UART_BAUDRATE))
    except Exception as err:
        UART_DEV = None
        if UART_PRINT_ERRORS and not UART_ERROR_PRINTED:
            print("uart output init failed: %s" % err)
            UART_ERROR_PRINTED = True

    return UART_DEV


def target_output_line(target):
    center_x, center_y = frame_center()

    if not target:
        return "TV,0,0,0,0,0,LOST\n"

    dx = target["x"] - center_x
    dy = target["y"] - center_y
    return "TV,1,%d,%d,%d,%d,%s\n" % (
        dx,
        dy,
        target["x"],
        target["y"],
        target["type"],
    )


def aim_output_line(target, laser):
    if target and laser:
        dx = target["x"] - laser["x"]
        dy = target["y"] - laser["y"]
        return "AIM,1,%d,%d,%d,%d,%d,%d,%s,%s\n" % (
            dx,
            dy,
            target["x"],
            target["y"],
            laser["x"],
            laser["y"],
            target["type"],
            LASER_COLOR,
        )

    if target:
        return "AIM,0,0,0,%d,%d,0,0,%s,NO_LASER\n" % (
            target["x"],
            target["y"],
            target["type"],
        )

    if laser:
        return "AIM,0,0,0,0,0,%d,%d,NO_TARGET,%s\n" % (
            laser["x"],
            laser["y"],
            LASER_COLOR,
        )

    return "AIM,0,0,0,0,0,0,0,LOST,LOST\n"


def uart_output_line(target, laser):
    if UART_OUTPUT_MODE == "target":
        return target_output_line(target)
    return aim_output_line(target, laser)


def send_uart_result(target, laser):
    global UART_ERROR_PRINTED

    if not ENABLE_UART_OUTPUT or not UART_DEV:
        return
    if UART_SEND_EVERY_N_FRAMES > 1 and state.FRAME_INDEX % UART_SEND_EVERY_N_FRAMES != 0:
        return

    try:
        UART_DEV.write_str(uart_output_line(target, laser))
    except Exception as err:
        if UART_PRINT_ERRORS and not UART_ERROR_PRINTED:
            print("uart output write failed: %s" % err)
            UART_ERROR_PRINTED = True
