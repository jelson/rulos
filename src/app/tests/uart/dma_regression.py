#!/usr/bin/env python3
"""Run against stm32_uart_regression on an H5/G4 with UART and native USB.

The firmware runs at 115200 baud. Reset the board before each invocation.
Example: python3 dma_regression.py --usb /dev/ttyACM0 --uart /dev/ttyACM2
"""

import argparse
import time
import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--usb", required=True)
    parser.add_argument("--uart", required=True)
    args = parser.parse_args()
    with serial.Serial(args.uart, 115200, timeout=1, write_timeout=1) as uart, serial.Serial(
        args.usb, 115200, timeout=1, write_timeout=1
    ) as usb:
        time.sleep(0.1)
        usb.reset_input_buffer()

        def command(text):
            usb.write(text.encode() + b"\n")
            response = usb.readline().decode().strip()
            assert response, f"No USB response to {text!r}"
            time.sleep(0.02)
            return response

        print(command("I"))
        uart.reset_input_buffer()
        assert command("T") == "TX"
        assert uart.readline() == b"UART-TX-OK\n"
        assert command("R") == "RESET"
        marker = b"hello-uart\n"
        uart.write(marker)
        uart.flush()
        time.sleep(0.03)
        assert bytes.fromhex(command("Q").split(" HEX ")[1]) == marker
        print("PASS: UART TX and DMA RX baseline")

        assert command("E") == "DMA PAUSED"
        uart.write(b"Z")
        uart.flush()
        time.sleep(0.03)
        assert command("N") == "RXNE 1", "UART ISR consumed a byte owned by RX DMA"
        print("PASS: IDLE ISR leaves DMA-owned RXNE data alone")


if __name__ == "__main__":
    main()
