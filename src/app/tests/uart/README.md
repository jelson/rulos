# UART Regressions

Run from the repository root. The host tests need a C compiler and Python 3:

```sh
python3 src/app/tests/uart/run_tests.py
python3 src/app/tests/core/run_tests.py
```

The UART host suite exercises the actual common UART receive code and the
shared STM32 DMA IRQ dispatcher. It checks callback data ownership, pending
HT/TC ordering in both buffer halves, disabled HT interrupts, cancellation
from a callback, and boundaries arriving during dispatch.

## Datalogger RAM

RX storage is caller-owned: only UARTs that receive data allocate a
`UartRxBuffer_t`. Its DMA/interrupt buffer is `UART_RX_QUEUE_LEN` bytes,
and its deferred-callback snapshot is half that size. TX-only consoles
must not allocate RX storage. The host suite checks this layout with the
default queue length and the dataloggers' 1536- and 8192-byte overrides.

Build all four dataloggers in an isolated directory without changing the
working revision. Put the configured ARM toolchain on `PATH` for the size
command, or use its absolute path:

```sh
build_dir=/tmp/rulos-uart-ram
scons -C src/app/datalogger --build-dir="$build_dir" -j4
arm-none-eabi-size -A \
  "$build_dir/solo-logger/arm-stm32g031x6/solo-logger.elf" \
  "$build_dir/gps-test-rig/arm-stm32g0b1xe/gps-test-rig.elf" \
  "$build_dir/gemini-logger/arm-stm32g0b1xe/gemini-logger.elf" \
  "$build_dir/ltetag-dev1/arm-stm32g0b1xe/ltetag-dev1.elf"
```

Count `.data`, `.bss`, `.noinit`, and `._user_heap_stack` when measuring
occupied RAM. The generated linker scripts reserve zero stack and heap
space, so remaining RAM is the actual space available to both, not a
margin beyond a stack reservation. The adjacent `.map` files show each
UART object's contribution.

Measured with GCC 15.2.1 when introducing explicit RX storage:

| Application | RAM capacity | Previous RAM used | New RAM used | New RAM left |
| --- | ---: | ---: | ---: | ---: |
| solo-logger | 8192 | 9792 | 5952 | 2240 |
| gps-test-rig | 131072 | 72024 | 43352 | 87720 |
| gemini-logger | 131072 | 89032 | 56264 | 74808 |
| ltetag-dev1 | 131072 | 37472 | 16992 | 114080 |

All figures are bytes. "Previous" means the full-snapshot layout at
`4b959c86`, which fails to link solo-logger. Simply halving the two embedded
snapshots would still leave solo-logger 64 bytes over its RAM limit.

## H5/G4 Hardware

`stm32_uart_regression` uses native USB for control and USART1 at 115200 baud
for test traffic. Connect the host UART adapter's TX to USART1 RX, RX to TX,
and ground to ground. The test uses the chip's default internal clock setup
and does not need an external reference.

Build the H523 version (replace `stm32h523xc` with `stm32g431x8` for G4):

```sh
scons -C src/app/tests/uart "$PWD/build/stm32_uart_regression/arm-stm32h523xc/stm32_uart_regression.elf"
```

Flashing replaces the board's application. Preserve any firmware that must
be restored afterward. With a Black Magic Probe attached:

```sh
python3 src/util/bmpflash.py build/stm32_uart_regression/arm-stm32h523xc/stm32_uart_regression.elf
python3 src/app/tests/uart/dma_regression.py --usb /dev/ttyACM0 --uart /dev/ttyACM2
```

Use the actual native USB and UART adapter device paths. Reset before each
run, including repeated runs of the same firmware:

```sh
python3 src/util/bmpflash.py --reset-only
```

The hardware suite checks TX/RX, RXNE ownership during an IDLE interrupt,
callback data retention while circular DMA overwrites the receive buffer,
reception after simultaneous HT/TC flags in either half, and interrupt-mask
preservation when acknowledging a pending IDLE batch. The delayed-IRQ cases
verify both flags were pending before allowing dispatch. They intentionally
exceed the receiver's buffering capacity: dropped overload traffic is
expected, but corrupting an outstanding callback or skipping later traffic
is not.
