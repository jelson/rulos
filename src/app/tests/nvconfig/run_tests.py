#!/usr/bin/env python3
"""Run actual H5 flash and timestamper persistence sources against an injectable flash model."""

from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
LIB = ROOT / "src/lib"
APP = ROOT / "src/app/timestamper"
SCPI_TEST = HERE.parent / "scpi"
USB = ROOT / "ext/stm32/STM32CubeG4/Middlewares/ST/STM32_USB_Device_Library"

with tempfile.TemporaryDirectory(prefix="rulos-nvconfig-") as build:
    cases = [
        ("backend", "backend_test.c", [], 16384, []),
        ("no_reserve", "backend_test.c", [], 0, []),
        ("small_reserve", "backend_test.c", [], 8192, []),
        ("small_sector", "backend_test.c", [], 16384, ["-DFLASH_SECTOR_SIZE=16"]),
        (
            "config",
            "config_test.c",
            [
                APP / "config.c",
                APP / "scpi.c",
                SCPI_TEST / "usb_fixture.c",
                LIB / "periph/scpi/scpi.c",
                LIB / "periph/uart/linereader.c",
                LIB / "core/queue.c",
                LIB / "chip/arm/stm32/periph/usb_cdc/usb_cdc.c",
            ],
            16384,
            ["-DGIT_COMMIT=test", "-Wno-unused-parameter"],
        ),
    ]
    for name, test, sources, reserve, flags in cases:
        binary = Path(build) / name
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-fno-pie",
                "-no-pie",
                "-DRULOS_ARM_stm32h5",
                "-ffunction-sections",
                "-fdata-sections",
                "-Wl,--gc-sections",
                f"-Wl,--defsym,_nvconfig_size={reserve}",
                "-I" + str(HERE / "stubs"),
                "-I" + str(SCPI_TEST / "stubs"),
                "-I" + str(LIB),
                "-I" + str(APP),
                "-I" + str(LIB / "chip/arm/stm32"),
                *[
                    "-I" + str(p)
                    for p in (
                        SCPI_TEST,
                        LIB / "chip/sim",
                        USB / "Core/Inc",
                        USB / "Class/CDC/Inc",
                    )
                ],
                *flags,
                str(HERE / test),
                str(LIB / "core/crc32.c"),
                *map(str, sources),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True, timeout=10)
