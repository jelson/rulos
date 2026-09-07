#!/usr/bin/env python3
"""Run host storage regressions against the production sources: python3 run_tests.py."""

from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
LIB = ROOT / "src/lib"

with tempfile.TemporaryDirectory(prefix="rulos-fat-") as build:
    for test in sorted(HERE.glob("*_test.c")):
        binary = Path(build) / test.stem
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-O2",
                "-Wall",
                "-Wextra",
                "-DRULOS_ARM_stm32f3",
                "-DBOARD_RULOS_AUDIO_REV_B",
                "-I" + str(HERE / "stubs"),
                "-I" + str(LIB),
                "-I" + str(LIB / "chip/sim"),
                "-I" + str(ROOT / "ext"),
                str(test),
                str(LIB / "core/time.c"),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True, timeout=10)
