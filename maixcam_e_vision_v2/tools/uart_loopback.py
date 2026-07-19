from drivers.uart_device import create_uart


def main():
    uart = create_uart()
    uart.write_str("AIM,0,0,0,0,0,0,0,V2_TEST,V2_TEST\n")
    print("V2 UART0 test frame sent")


if __name__ == "__main__":
    main()
