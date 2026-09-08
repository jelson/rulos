#!/usr/bin/env python3
"""Run host UART/DMA regressions: python3 run_tests.py."""

from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
LIB = HERE.parents[2] / "lib"

with tempfile.TemporaryDirectory(prefix="rulos-uart-") as build:
    state_sizes = set()
    for test in sorted(HERE.glob("*_test.c")):
        sources = [test]
        if test.stem == "receive_test":
            sources += [LIB / "periph/uart/uart.c", LIB / "core/queue.c"]
        if test.stem == "esp32_receive_test":
            sources += [LIB / "chip/esp32/periph/uart/uart.c"]
        queue_lengths = (
            (2, 64, 1536, 8192) if test.stem in ("receive_test", "esp32_receive_test") else (64,)
        )
        for queue_len in queue_lengths:
            binary = Path(build) / f"{test.stem}-{queue_len}"
            subprocess.run(
                [
                    "cc",
                    "-std=c11",
                    "-O2",
                    "-Wall",
                    "-Wextra",
                    "-ffunction-sections",
                    "-fdata-sections",
                    "-Wl,--gc-sections",
                    f"-DUART_RX_QUEUE_LEN={queue_len}",
                    "-I" + str(HERE / "stubs"),
                    "-I" + str(LIB),
                    "-I" + str(LIB / "chip/sim"),
                    *map(str, sources),
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], check=True)
            if test.stem == "receive_test":
                state_sizes.add(int(subprocess.check_output([str(binary), "--state-size"])))
    assert len(state_sizes) == 1, f"UartState_t size depends on RX queue length: {state_sizes}"
    print(f"uart: TX-only state is {state_sizes.pop()} host bytes for every RX queue length")

    for queue_len in (0, 1, 3, 1535, 8193):
        result = subprocess.run(
            [
                "cc",
                "-std=c11",
                f"-DUART_RX_QUEUE_LEN={queue_len}",
                "-I" + str(LIB),
                "-I" + str(LIB / "chip/sim"),
                "-x",
                "c",
                "-fsyntax-only",
                "-",
            ],
            input='#include "periph/uart/uart.h"\n',
            text=True,
            capture_output=True,
        )
        assert result.returncode != 0, f"invalid UART RX queue length {queue_len} compiled"
        assert "UART_RX_QUEUE_LEN must be at least 2 and even" in result.stderr
    print("uart: zero, one-byte, and odd RX queue configurations fail compilation")
