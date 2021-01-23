# Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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

import glob
import os
from SCons.Script import *

PROJECT_ROOT = os.path.relpath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC_ROOT = os.path.join(PROJECT_ROOT, "src")

def die(msg):
    sys.stderr.write(msg+"\n")
    sys.exit(1)

def cglob(*kargs):
    globpath = tuple(list(kargs) + ["*.c"])
    return glob.glob(os.path.join(*globpath))

def template_free_cglob(*kargs):
    return [f for f in cglob(*kargs) if not f.endswith("_template.c")]

class Platform:
    def common_libs(self, target):
        src_dirs = []
        src_dirs.append(os.path.join(SRC_ROOT, "lib", "core"))
        for periph_name in target.peripherals:
            periph_common_dir = os.path.join(SRC_ROOT, "lib", "periph", periph_name)
            periph_platform_dir = os.path.join(SRC_ROOT, "lib", "chip", self.periph_dir(), periph_name)
            if (not os.path.exists(periph_common_dir) and not os.path.exists(periph_platform_dir)):
                die(f"Periph {periph_name} has no source paths (looked in {periph_common_dir}, {periph_platform_dir})")
            src_dirs.append(periph_common_dir)
            src_dirs.append(periph_platform_dir)
        print("SRC_DIRS ", src_dirs)
        src_files = sum([cglob(d) for d in src_dirs], [])

        src_files.extend(self.platform_lib_source_files())
        #print("SRC_FILES ", src_files)
        return src_files

    def common_cflags(self):
        return ["-Wall", "-Werror"
            #, "-flto"
            ]

    def common_include_dirs(self):
        return [os.path.join(SRC_ROOT, "lib")]
    
    def common_ld_flags(self, target):
        return [
            "-Wl,--fatal-warnings", # linker should fail on warning
            "-nostartfiles",
            "-Wl,--gc-sections",    # garbage-collect unused functions
            f"-Wl,-Map={target.name}.{self.name()}.{target.board}.map,--cref",
            "-static",
        ]

# TODO move knowledge to the platform-specific directories
class ArmPlatform(Platform):
    ARM_COMPILER_PREFIX = "/usr/local/bin/gcc-arm-none-eabi-9-2019-q4-major/bin/arm-none-eabi-"

    class Architecture:
        def __init__(self, name, arch, mcpu, more_cflags=[]):
            self.name = name
            self.arch = arch
            self.mcpu = mcpu
            self.flags = ["-march="+self.arch, "-mcpu="+self.mcpu] + more_cflags

    ARCHITECTURES = dict([(arch.name, arch) for arch in [
        Architecture("m0", "armv6-m", "cortex-m0"),
        Architecture("m0plus", "armv6-m", "cortex-m0plus"),
        Architecture("m3", "armv7-m", "cortex-m3"),
        Architecture("m4", "armv7e-m", "cortex-m4"),
        Architecture("m4f", "armv7e-m", "cortex-m4", ["-mfloat-abi=hard", "-mfpu=fpv4-sp-d16"]),
        ]])

    def __init__(self, chip_name):
        self.chip_name = chip_name

    def build(self, target):
        env = Environment()
        lib_obj_dir = os.path.join(PROJECT_ROOT, "build", "lib", self.name())
        env.Replace(CC = self.ARM_COMPILER_PREFIX+"gcc")
        env.Replace(AS = self.ARM_COMPILER_PREFIX+"as")
        env.Replace(AR = self.ARM_COMPILER_PREFIX+"ar")
        env.Replace(RANLIB = self.ARM_COMPILER_PREFIX+"ranlib")
    
        platform_lib_srcs = [
            os.path.join(lib_obj_dir, os.path.relpath(s, PROJECT_ROOT)) for s in self.common_libs(target)]
        env.Append(CFLAGS = self.cflags())
        env.Append(CFLAGS = target.cflags())
        env.Append(LINKFLAGS = self.cflags())
        env.Append(LINKFLAGS = target.cflags())
        env.Append(LINKFLAGS = self.ld_flags(target))
        env.Replace(CPPPATH = self.include_dirs())

        lib_env = env.Clone()
        # We're pretending the sources appear in lib_obj_dir, so they look adjacent to the build output.
        lib_env.VariantDir(lib_obj_dir, PROJECT_ROOT, duplicate=0)
        rocket_lib = lib_env.StaticLibrary(os.path.join(lib_obj_dir, "rocket"),
            source=platform_lib_srcs)

        app_env = env.Clone()
        app_obj_dir = os.path.join(PROJECT_ROOT, "build", target.name, self.name())
        # We're pretending the sources appear in app_obj_dir, so they look adjacent to the build output.
        app_env.VariantDir(app_obj_dir, PROJECT_ROOT, duplicate=0)
        target_sources = [
            os.path.join(app_obj_dir, os.path.relpath(".", PROJECT_ROOT), s) for s in target.sources]
        platform_app_sources = [
            os.path.join(app_obj_dir, os.path.relpath(s, PROJECT_ROOT)) for s in self.platform_specific_app_sources()]
        print("platform_app_sources", platform_app_sources)

        linkscript = self.make_linkscript(app_env, app_obj_dir)
        program_name = os.path.join(app_obj_dir, target.name+".elf")
        app_env.Depends(program_name, linkscript)

        app_binary = app_env.Program(program_name,
            source=target_sources + platform_app_sources + rocket_lib)
        Default(app_binary)

    def platform_cflags(self, arch):
        return self.common_cflags() + arch.flags + [
            f"-DCORE_{arch.name.upper()}=1",
            "-mthumb",
            "-DRULOS_ARM",
            "-D__HEAP_SIZE=0x0",
            "-D__STACK_SIZE=0x400",
            # Jeremy adds nano.specs because without it, we use ~1k of RAM on
            # a section called impure_data, which is gcc internal stuff.
            # See https://stackoverflow.com/questions/48711221/how-to-prevent-inclusion-of-c-library-destructors-and-atexit
            "--specs=nano.specs",
            "--specs=nosys.specs",
            "-fdata-sections", # Put all funcs/data in their own sections
            "-ffunction-sections",
            "-std=gnu99",
            "-g",
            "-O2",
#            "-D__STACK_SIZE=$(STACK_SIZE)",
#            "-D__HEAP_SIZE=$(HEAP_SIZE)",
            ]

    def arm_ld_flags(self, target):
        return self.common_ld_flags(target)

    ARM_ROOT = os.path.join(SRC_ROOT, "lib", "chip", "arm")
    def platform_include_dirs(self):
        return self.common_include_dirs() + [
            os.path.join(self.ARM_ROOT, "common"),
            os.path.join(self.ARM_ROOT, "common", "CMSIS", "Include"),
            ]

    def arm_platform_lib_source_files(self):
        return cglob(self.ARM_ROOT, "common", "core")

    def name(self):
        return f"arm-{self.part_name()}"

STM32_ROOT = os.path.join(ArmPlatform.ARM_ROOT, "stm32")
class ArmStmPlatform(ArmPlatform):
    class Chip:
        def __init__(self, name, family, flashk, ramk):
            self.name = name
            self.family = family
            self.flashk = flashk
            self.ramk = ramk
            self.major_family_name = family[:7]

    CHIPS = dict([(chip.name, chip) for chip in [
        Chip("stm32g030x6", "STM32G030xx", flashk=32, ramk=8),
        Chip("stm32g030x8", "STM32G030xx", flashk=64, ramk=8),
    ]])

    class MajorFamily:
        def __init__(self, name, arch_name):
            self.name = name
            self.arch = ArmPlatform.ARCHITECTURES[arch_name]
            driver_root = os.path.join(STM32_ROOT, "vendor_libraries", name.lower(), "Drivers")
            self.cmsis_root = os.path.join(driver_root, "CMSIS", "Device", "ST", name+"xx")
            self.hal_root = os.path.join(driver_root, name+"xx_HAL_Driver")
            self.sources = os.path.join(self.cmsis_root, "Source", "Templates", "system_"+name.lower()+"xx.c")

    MAJOR_FAMILIES = dict([(fam.name, fam) for fam in [
        MajorFamily("STM32G0", "m0plus"),
        MajorFamily("STM32G4", "m4f"),
    ]])

    def __init__(self, chip_name):
        super(ArmStmPlatform, self).__init__(chip_name)
        if (chip_name not in self.CHIPS):
            die(f"Unrecognized chip {chip}")
        self.chip = self.CHIPS[self.chip_name]
        self.major_family = self.MAJOR_FAMILIES[self.chip.major_family_name]

    def part_name(self):
        return self.chip_name

    def periph_dir(self):
        return os.path.join("arm", "stm32")

    def cflags(self):
        return self.platform_cflags(self.major_family.arch) + [
            "-DRULOS_ARM_STM32",
            "-DUSE_FULL_LL_DRIVER",
            "-DUSE_HAL_DRIVER",
            "-DRULOS_ARM_"+self.chip.major_family_name.lower(),
            f"-D{self.chip.family}=1",
            ]

    def ld_flags(self, target):
        return self.arm_ld_flags(target)
#    def 
#-T obj.nulltest.arm-stm32g030x8.GENERIC/stm32g030x8-linker-script.ld

    def include_dirs(self):
        return self.platform_include_dirs() + [
            STM32_ROOT,
            os.path.join(self.major_family.cmsis_root, "Include"),
            os.path.join(self.major_family.hal_root, "Inc"),
            ]

    def platform_lib_source_files(self):
        return (self.arm_platform_lib_source_files()
            + cglob(STM32_ROOT, "core")
            + template_free_cglob(self.major_family.hal_root, "Src")
            + [self.major_family.sources])

    def platform_specific_app_sources(self):
        return [
            os.path.join(self.major_family.cmsis_root, "Source", "Templates", "gcc",
                f"startup_{self.chip.family.lower()}.s"),
            ]

    def make_linkscript(self, env, app_obj_dir):
        linkscript_name = f"{self.chip.name}-linker-script.ld"
        linkscript = env.Command(
            source = os.path.join(app_obj_dir, STM32_ROOT, "linker", "stm32-generic.ld"),
            target = os.path.join(app_obj_dir, linkscript_name),
            action = f'sed "s/%RULOS_FLASHK%/{self.chip.flashk}/; s/%RULOS_RAMK%/{self.chip.ramk}/" $SOURCE > $TARGET')
        env.Append(LINKFLAGS = ["-T", linkscript])
        return linkscript

class AvrPlatform(Platform):
    def __init__(self, mcu):
        self.mcu = mcu

    def name(self):
        return f"avr-{self.mcu}"

    def periph_dir(self):
        return "avr"

class SimPlatform(Platform):
    def __init__(self):
        pass

    def name(self):
        return "sim"

class RulosBuildTarget:
    def __init__(self, name, sources, platforms, peripherals = [], board = "GENERIC"):
        self.name = name
        self.sources = sources
        self.platforms = platforms
        assert(type(self.platforms)==type([]))
        for p in self.platforms:
            assert(isinstance(p, Platform))
        self.peripherals = peripherals
        self.board = board

    def build(self):
        for platform in self.platforms:
            platform.build(self)

    def cflags(self):
        return [f"-DBOARD_{self.board}"]
