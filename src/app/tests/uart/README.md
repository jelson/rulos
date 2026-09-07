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
