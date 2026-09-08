#!/usr/bin/env python3
"""Run production pulsegen timing regressions without hardware."""

from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
APP = HERE.parents[1] / "pulsegen"

with tempfile.TemporaryDirectory(prefix="rulos-pulsegen-") as build:
    for test in sorted(HERE.glob("*_test.c")):
        binary = Path(build) / test.stem
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
                "-I" + str(APP),
                str(test),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
