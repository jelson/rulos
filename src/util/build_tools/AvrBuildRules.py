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

import os
from . import BaseRules, util
from SCons.Script import *

class AvrPlatform(BaseRules.Platform):
    def __init__(self, mcu, extra_peripherals = [], extra_cflags = []):
        super().__init__(extra_peripherals, extra_cflags)
        self.mcu = mcu

    def name(self):
        return f"avr-{self.mcu}"

    def target_suffix(self):
        return ".elf"

    def periph_dir(self):
        return os.path.join("avr", "periph")

    def configure_env(self, env):
        self.configure_compiler(env, "avr-")

    def platform_lib_source_files(self):
        return util.cglob(util.SRC_ROOT, "lib", "chip", "avr", "core")

    def cflags(self):
        return self.common_cflags() + [
            "-mmcu="+self.mcu,
            f"-DMCU{self.mcu}=1",
            "-DRULOS_AVR",
            "-funsigned-char",
            "-funsigned-bitfields",
            "-fpack-struct",
            "-fshort-enums",
            "-fdata-sections",
            "-ffunction-sections", # Put all funcs/data in their own sections
            "-gdwarf-2",
            "-Os",
        ]

    def include_dirs(self):
        return self.common_include_dirs() + [ os.path.join(util.SRC_ROOT, "lib", "chip", "avr") ]

    def ld_flags(self, target):
        return self.common_ld_flags(target)

    def platform_specific_app_sources(self):
        return []

    def post_configure(self, env, outputs):
        HEX_FLASH_FLAGS = "-R .eeprom -R .fuse -R .lock -R .signature"
        env.Append(BUILDERS = {"MakeHex": Builder(
            src_suffix = ".elf",
            suffix = ".hex",
            action = f"avr-objcopy -O ihex {HEX_FLASH_FLAGS}  $SOURCE $TARGET")})
        Default(env.MakeHex(outputs[0]))
