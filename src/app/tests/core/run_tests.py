#!/usr/bin/env python3
"""Run host regressions against the actual core sources: python3 run_tests.py."""

from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
LIB = HERE.parents[2] / "lib"

# Core sources each test needs beyond its namesake.
EXTRA_SOURCES = {
    "heap_test": ["time.c"],
    "time_test": ["clock.c", "heap.c"],
}

with tempfile.TemporaryDirectory(prefix="rulos-core-") as build:
    for test in sorted(HERE.glob("*_test.c")):
        binary = Path(build) / test.stem
        sources = [test, LIB / "core" / (test.stem.removesuffix("_test") + ".c")]
        sources += [LIB / "core" / dep for dep in EXTRA_SOURCES.get(test.stem, [])]
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-O2",
                "-Wall",
                "-Wextra",
                "-I" + str(LIB),
                "-I" + str(LIB / "chip/sim"),
                *map(str, sources),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
