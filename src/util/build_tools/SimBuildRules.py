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

class SimulatorPlatform(BaseRules.Platform):
    def __init__(self, extra_peripherals = [], extra_cflags = []):
        super().__init__(extra_peripherals, extra_cflags)

        # Simulator always requires uart peripheral since it's involved with
        # logging
        self.extra_peripherals.append("uart")

    def name(self):
        return "simulator"

    def target_suffix(self):
        return "sim"

    def configure_env(self, env):
        self.configure_compiler(env, "")

        env.Append(LIBS = ["m", "ncurses"])

        # needed for 6-matrix
        #env.Append(LIBS = pkgconfig("--libs", "gtk+-2.0"))

    def periph_dir(self):
        return os.path.join("sim", "periph")

    def platform_lib_source_files(self):
        return util.cglob(util.SRC_ROOT, "lib", "chip", "sim", "core")

    def cflags(self):
        return self.common_cflags() + [
            "-DSIM"
        ]
        # add for 6matrix: pkgconfig("--cflags", "gtk+-2.0")

    def ld_flags(self, target):
        return  self.common_ld_flags(target)

    def include_dirs(self, target):
        return self.common_include_dirs(target) + [
            os.path.join(util.SRC_ROOT, "lib", "chip", "sim")
        ]

    def platform_specific_app_sources(self):
        return []

    def post_configure(self, env, app_binary):
        pass
