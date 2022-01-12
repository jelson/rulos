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

from .util import *
from . import SpecialRules
from SCons.Script import *

class Platform:
    def __init__(self, extra_peripherals, extra_cflags):
        self.extra_peripherals = parse_peripherals(extra_peripherals)
        self.extra_cflags = extra_cflags

    def configure_compiler(self, env, compiler_prefix):
        env.Replace(CC = compiler_prefix+"g++")
        env.Replace(CXX = compiler_prefix+"g++")
        env.Replace(AS = compiler_prefix+"as")
        env.Replace(AR = compiler_prefix+"gcc-ar")
        env.Replace(RANLIB = compiler_prefix+"gcc-ranlib")

        env.Append(BUILDERS = {
            "MakeLSS": Builder(
                src_suffix = ".elf",
                suffix = ".lss",
                action = compiler_prefix+"objdump -h -S --syms $SOURCE > $TARGET",
            ),
            "ShowSizes": Builder(
                src_suffix = ".elf",
                suffix = "show_sizes",
                action = compiler_prefix+"size $SOURCE",
            )
        })

    def common_libs(self, target):
        src_files = []

        # add all hardware-agnostic core source files
        src_files.extend(cglob(os.path.join(SRC_ROOT, "lib", "core")))

        # add core source files for this hardware platform
        src_files.extend(self.platform_lib_source_files())

        # add source files for all peripherals requested by this app
        for periph_name in target.peripherals + self.extra_peripherals:
            # First check if the peripheral has special build rules
            if periph_name in SpecialRules.PERIPHERALS:
                src_files.extend(SpecialRules.PERIPHERALS[periph_name]['src'])
            else:
                # check both the hardware-agnostic and hardware-specific peripheral
                # directories for a subdirectory matching the peripheral name
                periph_dirs = [
                    os.path.join(SRC_ROOT, "lib", "periph", periph_name),
                    os.path.join(SRC_ROOT, "lib", "chip", self.periph_dir(), periph_name),
                ]
                dir_found = False
                for periph_dir in periph_dirs:
                    if os.path.exists(periph_dir):
                        dir_found = True
                        src_files.extend(cglob(periph_dir))
                if not dir_found:
                    die(f"Periph {periph_name} has no source paths (looked in {periph_dirs})")

        return src_files

    def common_cflags(self):
        return self.extra_cflags + [
            "-Wall",
            "-Werror",
            "-g",
            "-flto",
            "-std=gnu++11",
        ]

    def common_include_dirs(self):
        return [
            os.path.join(SRC_ROOT, "lib"),
            ".", # app source code directory
        ]

    def common_ld_flags(self, target):
        return [
            "-Wl,--fatal-warnings", # linker should fail on warning
            "-Wl,--gc-sections",    # garbage-collect unused functions
        ]

    def build(self, target):
        env = Environment()

        # We're pretending the sources appear in build_obj_dir, so they look
        # adjacent to the build output.
        build_obj_dir = os.path.join(BUILD_ROOT, target.name, self.name())
        env.VariantDir(build_obj_dir, PROJECT_ROOT, duplicate=0)

        program_name = os.path.join(build_obj_dir, target.name+self.target_suffix())

        # Export names & paths to subclasses and converter actions
        env.Replace(RulosBuildObjDir = build_obj_dir)
        env.Replace(RulosProgramName = target.name)
        env.Replace(RulosBinaryPath = program_name)
        env.Replace(RulosProjectRoot = PROJECT_ROOT)

        self.configure_env(env)

        platform_lib_srcs = [os.path.join(build_obj_dir, s) for s in self.common_libs(target)]
        env.Append(CCFLAGS = self.cflags())
        env.Append(CCFLAGS = target.cflags())
        env.Append(LINKFLAGS = self.cflags())
        env.Append(LINKFLAGS = target.cflags())
        env.Append(LINKFLAGS = self.ld_flags(target))
        env.Append(CPPPATH = self.include_dirs())
        env.Append(CPPPATH = os.path.join(build_obj_dir, "src"))

        # Add a linker flag so gcc generates a map, and modify the standard
        # Program builder to know that map generation is a side-effect of
        # compilation
        map_filename = os.path.join(build_obj_dir, f"{target.name}.map")
        env.Append(LINKFLAGS = f"-Wl,-Map={map_filename},--cref")
        def map_emitter(scons_target, source, env):
            assert(len(scons_target) == 1)
            assert(str(scons_target[0]) == os.path.abspath(program_name))
            scons_target.append(map_filename)
            return scons_target, source
        env.Append(PROGEMITTER = [map_emitter])

        # Build the lib
        rocket_lib = env.StaticLibrary(os.path.join(build_obj_dir, "src", "lib", "rocket"),
            source=platform_lib_srcs)
        for converter in SpecialRules.CONVERTERS + target.extra_converters:
            converter_output = env.Command(
                source = [os.path.join(build_obj_dir, p) for p in converter.script_input],
                target = os.path.join(build_obj_dir, converter.intermediate_file),
                action = converter.action,
            )
            env.Depends(
                os.path.join(build_obj_dir, converter.dependent_source),
                os.path.join(build_obj_dir, converter.intermediate_file))

        # Build the app
        target_sources = [
            os.path.join(build_obj_dir, s) for s in
            target.sources + self.platform_specific_app_sources()]

        app_binary = env.Program(env["RulosBinaryPath"],
                                 source=target_sources + rocket_lib)
        lss = env.MakeLSS(app_binary)
        sizes = env.ShowSizes(app_binary)
        outputs = [app_binary, lss, sizes]
        self.post_configure(env, outputs)
        Default(outputs)
