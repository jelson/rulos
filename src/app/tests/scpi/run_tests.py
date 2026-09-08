#!/usr/bin/env python3
"""Run SCPI/CDC regressions against production sources: python3 run_tests.py."""

from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
LIB = ROOT / "src/lib"
USB = ROOT / "ext/stm32/STM32CubeG4/Middlewares/ST/STM32_USB_Device_Library"

with tempfile.TemporaryDirectory(prefix="rulos-scpi-") as build:
    for test in sorted(HERE.glob("*_test.c")):
        binary = Path(build) / test.stem
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Wno-unused-parameter",
                "-ffunction-sections",
                "-fdata-sections",
                "-Wl,--gc-sections",
                "-DRULOS_ARM_stm32g4",
                "-DGIT_COMMIT=test",
                *[
                    "-I" + str(p)
                    for p in (
                        HERE / "stubs",
                        LIB,
                        LIB / "chip/sim",
                        LIB / "chip/arm/stm32",
                        USB / "Core/Inc",
                        USB / "Class/CDC/Inc",
                    )
                ],
                str(test),
                str(HERE / "usb_fixture.c"),
                str(LIB / "periph/scpi/scpi.c"),
                str(LIB / "periph/uart/linereader.c"),
                str(LIB / "core/queue.c"),
                str(LIB / "chip/arm/stm32/periph/usb_cdc/usb_cdc.c"),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True, timeout=10)
