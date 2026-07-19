"""UART0 setup with explicit ownership guard."""

from settings import UART_BAUDRATE, UART_ENABLE_RX, UART_PORT, UART_REQUIRE_COMM_NONE, UART_RX_FUNC, UART_RX_PIN, UART_TX_FUNC, UART_TX_PIN


def create_uart():
    from maix import app, err, pinmap, uart
    if UART_REQUIRE_COMM_NONE and app.get_sys_config_kv("comm", "method") != "none":
        raise RuntimeError("Set maix_comm_method=none in /boot/configs and reboot before using UART0")
    err.check_raise(pinmap.set_pin_function(UART_TX_PIN, UART_TX_FUNC), "UART TX mapping failed")
    if UART_ENABLE_RX:
        err.check_raise(pinmap.set_pin_function(UART_RX_PIN, UART_RX_FUNC), "UART RX mapping failed")
    return uart.UART(UART_PORT, UART_BAUDRATE)
