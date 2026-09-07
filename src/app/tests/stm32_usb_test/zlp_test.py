#!/usr/bin/env python3
"""Regression for main.c: a USB OUT ZLP must not stop later reception.

Flash stm32_libusb_test first. Requires pyserial, pyusb, a UART connection
at 1 Mbps, and raw USB device permissions. Kernel drivers are reattached
on exit; reset the target after an expected failure on unfixed firmware.
"""

import argparse
import time

import serial
import usb.core
import usb.util


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uart", required=True)
    parser.add_argument("--vid", type=lambda s: int(s, 0), default=0x0424)
    parser.add_argument("--pid", type=lambda s: int(s, 0), default=0x274E)
    parser.add_argument("--serial", help="USB serial number, required if multiple devices match")
    args = parser.parse_args()
    devices = list(usb.core.find(find_all=True, idVendor=args.vid, idProduct=args.pid))
    if args.serial:
        devices = [d for d in devices if d.serial_number == args.serial]
    if len(devices) != 1:
        parser.error(f"Expected one target, found {len(devices)}")
    device = devices[0]
    detached = []
    claimed = []
    try:
        data = next(i for i in device.get_active_configuration() if i.bInterfaceClass == 0x0A)
        endpoint = next(
            e
            for e in data
            if usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT
        )
        for interface in (0, data.bInterfaceNumber):
            if device.is_kernel_driver_active(interface):
                device.detach_kernel_driver(interface)
                detached.append(interface)
            usb.util.claim_interface(device, interface)
            claimed.append(interface)
        with serial.Serial(args.uart, 1000000, timeout=0.05) as uart:
            for trial in range(4):
                if trial:
                    assert endpoint.write(b"", timeout=1000) == 0
                marker = f"rulos-zlp-marker-{trial}".encode()
                uart.reset_input_buffer()
                assert endpoint.write(marker, timeout=1000) == len(marker)
                received = bytearray()
                deadline = time.monotonic() + 2
                while marker not in received and time.monotonic() < deadline:
                    received.extend(uart.read(uart.in_waiting or 1))
                assert marker in received, f"UART did not report USB RX: {received!r}"
                print(f"PASS: received marker {trial}" + (" after ZLP" if trial else " before ZLP"))
    finally:
        for interface in reversed(claimed):
            usb.util.release_interface(device, interface)
        for interface in detached:
            device.attach_kernel_driver(interface)
        usb.util.dispose_resources(device)


if __name__ == "__main__":
    main()
