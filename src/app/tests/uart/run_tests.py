#!/usr/bin/env python3
"""Run host UART/DMA regressions: python3 run_tests.py."""

from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
LIB = HERE.parents[2] / "lib"

with tempfile.TemporaryDirectory(prefix="rulos-uart-") as build:
    for test in sorted(HERE.glob("*_test.c")):
        binary = Path(build) / test.stem
        sources = [test]
        if test.stem == "receive_test":
            sources += [LIB / "periph/uart/uart.c", LIB / "core/queue.c"]
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
                "-I" + str(LIB),
                "-I" + str(LIB / "chip/sim"),
                *map(str, sources),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
