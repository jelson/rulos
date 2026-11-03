# Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
# (jelson@gmail.com).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# Espressif-based boards RULOS knows: the chip plus the sdkconfig that
# describes the module around it (flash, PSRAM, console). Pin assignments
# live in the matching C header under src/lib/chip/esp32/boards/.

import dataclasses


@dataclasses.dataclass(frozen=True)
class Esp32Board:
    name: str
    chip: str
    # sdkconfig entries, keyed without the CONFIG_ prefix
    sdkconfig: dict
    # The flash layout in ESP-IDF's partition-table CSV form, or None for
    # IDF's default single-app layout, which only suits 4 MB parts.
    partition_table: str = None


# Waveshare ESP32-S3-Touch-LCD-4.3B: ESP32-S3-WROOM-1-N16R8 (16 MB QIO
# flash, 8 MB octal PSRAM), 800x480 RGB panel, native USB on the USB-C
# connector and no USB-UART bridge.
WAVESHARE_ESP32_S3_TOUCH_LCD_43B = Esp32Board(
    name="waveshare-esp32-s3-touch-lcd-4.3b",
    chip="esp32s3",
    sdkconfig={
        "ESPTOOLPY_FLASHSIZE_16MB": True,
        "ESPTOOLPY_FLASHMODE_QIO": True,
        "ESP_DEFAULT_CPU_FREQ_MHZ_240": True,
        "SPIRAM": True,
        "SPIRAM_MODE_OCT": True,
        "SPIRAM_SPEED_80M": True,
        # run code and constants from PSRAM so a flash operation never
        # starves the panel's DMA
        "SPIRAM_FETCH_INSTRUCTIONS": True,
        "SPIRAM_RODATA": True,
        "ESP32S3_DATA_CACHE_LINE_64B": True,
        "LCD_RGB_ISR_IRAM_SAFE": True,
        "LCD_RGB_RESTART_IN_VSYNC": True,
        # panics and IDF logs go out the native USB, the only console
        "ESP_CONSOLE_USB_SERIAL_JTAG": True,
    },
    # One 4 MB app; the rest of the 16 MB is free for a filesystem later.
    partition_table="""\
# Name,     Type, SubType,  Offset, Size
nvs,        data, nvs,      ,       24K
phy_init,   data, phy,      ,       4K
factory,    app,  factory,  ,       4M
""",
)
