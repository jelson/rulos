#!/usr/bin/env python3
"""Run production timestamper capture regressions without hardware."""

from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
APP = HERE.parents[1] / "timestamper"

with tempfile.TemporaryDirectory(prefix="rulos-timestamper-") as build:
    for test in sorted(HERE.glob("*_test.c")):
        for dma_length, ring_length in ((16, 32), (2048, 16384)):
            binary = Path(build) / f"{test.stem}-{dma_length}-{ring_length}"
            subprocess.run(
                [
                    "cc",
                    "-std=c11",
                    "-O2",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-fsanitize=undefined,bounds",
                    "-fno-sanitize-recover=all",
                    f"-DDMA_CAPTURE_BUFLEN={dma_length}U",
                    f"-DTIMESTAMP_BUFLEN={ring_length}U",
                    "-I" + str(APP),
                    str(test),
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], check=True)
