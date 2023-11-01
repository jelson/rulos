from .util import *
import glob
import os

class Converter:
    def __init__(self, dependent_source, intermediate_file, action, script_input):
        self.dependent_source = dependent_source
        self.intermediate_file = intermediate_file
        self.action = action
        self.script_input = script_input

CONVERTERS = [
    Converter(
        dependent_source = "src/lib/periph/7seg_panel/display_controller.c",
        intermediate_file = "src/lib/periph/7seg_panel/sevseg_bitmaps.ch",
        action = "$RulosProjectRoot/src/util/sevseg_convert.py $SOURCE > $TARGET",
        script_input = ["src/lib/periph/7seg_panel/sevseg_artwork.txt"]
    ),
    Converter(
        dependent_source = "src/lib/periph/rasters/rasters.c",
        intermediate_file = "src/lib/periph/rasters/rasters_auto.ch",
        action = "$RulosProjectRoot/src/util/bitmaploader.py $TARGET $SOURCES > /dev/null",
        script_input = cwd_to_project_root(
            glob.glob(os.path.join(PROJECT_ROOT, "src", "bitmaps", "*.png")))
    ),
    Converter(
        dependent_source = "src/lib/chip/avr/periph/lcd_12232/hardware_graphic_lcd_12232.c",
        intermediate_file = "src/lcdrasters_auto.ch",
        action = "$RulosProjectRoot/src/util/lcdbitmaploader.py $TARGET $RulosProjectRoot/src/lcdbitmaps",
        script_input = cwd_to_project_root(
            glob.glob(os.path.join(PROJECT_ROOT, "src", "lcdbitmaps", "*.png")))
    ),
]

PERIPHERALS = {
    'usb-stm32g4': {
        'src': [
            'ext/stm32/STM32CubeG4/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c',
        ],
        'incdir': [
            'ext/stm32/STM32CubeG4/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc',
            'ext/stm32/STM32CubeG4/Middlewares/ST/STM32_USB_Device_Library/Core/Inc',
        ],
    },

    'usb-stm32-ext': {
        'src': [
            'src/lib/chip/arm/stm32/periph/libusb_stm32/src/usbd_core.c',
            'src/lib/chip/arm/stm32/periph/libusb_stm32/src/usbd_stm32l433_devfs.c',
        ],
        'incdir': [
            'src/lib/chip/arm/stm32/periph/libusb_stm32/inc',
        ],
    },
}
