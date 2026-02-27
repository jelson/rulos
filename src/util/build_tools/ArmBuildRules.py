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
import subprocess

from . import BaseRules, util
from SCons.Script import *

ARM_ROOT = os.path.join(util.SRC_ROOT, "lib", "chip", "arm")
STM32_ROOT = os.path.join(ARM_ROOT, "stm32")
STM32_VENDOR_ROOT = os.path.join(util.PROJECT_ROOT, "ext", "stm32")

created_programming_target = False

class ArmPlatform(BaseRules.Platform):
    def __init__(self, chip_name, extra_peripherals, extra_cflags):
        super().__init__(extra_peripherals, extra_cflags)
        self.chip_name = chip_name
        toolchain_dir = os.environ.get('ARM_TOOLCHAIN_DIR', '')
        if toolchain_dir and not os.path.exists(toolchain_dir):
            sys.exit(f'ARM_TOOLCHAIN_DIR directory {toolchain_dir} does not exist')
        test_path = os.path.join(toolchain_dir, 'arm-none-eabi-gcc')
        test_path_resolved = util.which(test_path)
        if not test_path_resolved:
            sys.exit(f"{test_path} not found; ensure it's in your path or set ARM_TOOLCHAIN_DIR")
        self.toolchain_prefix = os.path.join(os.path.dirname(test_path_resolved), 'arm-none-eabi-')

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
#            "-O0",
            "-O2",
#            "-D__STACK_SIZE=$(STACK_SIZE)",
#            "-D__HEAP_SIZE=$(HEAP_SIZE)",
            ]

    def arm_ld_flags(self, target):
        return self.common_ld_flags(target) + [
            "-nostartfiles",
            "-static",
        ]

    def platform_include_dirs(self, target):
        return self.common_include_dirs(target) + [
            os.path.join(ARM_ROOT, "common"),
            os.path.join(ARM_ROOT, "common", "CMSIS", "Include"),
        ]

    def arm_platform_lib_source_files(self):
        return util.cglob(ARM_ROOT, "common", "core")

    def name(self):
        return f"arm-{self.part_name()}"

    def target_suffix(self):
        return ".elf"

    def arm_configure_env(self, env):
        self.configure_compiler(env, self.toolchain_prefix)

    def programming_cmdline(self, usbpath, elfpath):
        if 'BMP_POWER' in os.environ:
            power = ['-ex', 'mon tpwr enable']
        else:
            power = []

        return [
            self.toolchain_prefix + 'gdb', elfpath,
            '-ex', 'set pagination off',
            '-ex', 'set confirm off',
            '-ex', f'target extended-remote {usbpath}',
        ] + power + [
            '-ex', 'monitor swd',
            '-ex', 'attach 1',
            '-ex', 'load',
            '-ex', 'quit',
        ]

    def find_bmp_port(self, name):
        import serial.tools.list_ports as list_ports

        bmp_ports = list(list_ports.grep(f'Black Magic {name}'))

        if len(bmp_ports) == 0:
            print("No Black Magic Probe found; is it plugged in?")
            return None

        if len(bmp_ports) > 1:
            print("Multiple Black Magic probes found! Unplug some.")
            return None

        return bmp_ports[0].device

    def program_with_bmp(self, target, source, env):
        elffile = source[0].get_abspath()

        gdb_server_path = self.find_bmp_port('GDB Server')

        if not gdb_server_path:
            return

        cmdline = self.programming_cmdline(gdb_server_path, elffile)
        print(cmdline)
        subprocess.call(cmdline)

    def read_bmp_serial(self, target, source, env):
        uart_path = self.find_bmp_port('UART Port')

        if not uart_path:
            return

        subprocess.call(["grabserial", "-d", uart_path, "-b", "1000000", "-t"])

    def post_configure(self, env, outputs):
        # Create a hex file from the elf file
        hexfile = env.HexFile(outputs[0])
        Default(hexfile)

        # Create programming aliases so a command like "scons
        # program-stm32-<progname>" works, even for SConstruct files
        # that have multiple targets
        prog_target = f"stm32-{os.path.basename(env['RulosTargetName'])}"
        program = env.Alias(f"program-{prog_target}", outputs, self.program_with_bmp)
        env.Alias(f"run-{prog_target}", program, self.read_bmp_serial)

        # If there's only one binary, create a simpler "scons program"
        # target that works without specifying the platform and binary
        # name
        global created_programming_target
        if not created_programming_target:
            env.Alias("program", f"program-{prog_target}")
            env.Alias("run", f"run-{prog_target}")
            created_programming_target = True

class ArmStmPlatform(ArmPlatform):
    def __init__(self, chip_name, extra_peripherals = [], extra_cflags = []):
        super().__init__(chip_name, extra_peripherals, extra_cflags)
        if chip_name not in self.CHIPS:
            util.die(f"Unrecognized chip {chip_name}. Known chips: " +
                     ",".join(self.CHIPS))
        self.chip = self.CHIPS[self.chip_name]
        self.major_family = self.MAJOR_FAMILIES[self.chip.major_family_name]

    class Chip:
        def __init__(self, name, family, flashk, ramk, ccmramk=0):
            self.name = name
            self.family = family
            self.flashk = flashk
            self.ramk = ramk
            self.ccmramk = ccmramk
            self.major_family_name = family[:7]

    CHIPS = dict([(chip.name, chip) for chip in [
        # stm32c0
        Chip("stm32c011x4", "STM32C011xx", flashk=  16, ramk=  6),
        Chip("stm32c011x6", "STM32C011xx", flashk=  32, ramk=  6),

        # stm32f0
        Chip("stm32f030x4", "STM32F030x6", flashk=  16, ramk=  4),
        Chip("stm32f030x6", "STM32F030x6", flashk=  32, ramk=  4),
        Chip("stm32f030x8", "STM32F030x8", flashk=  64, ramk=  8),
        Chip("stm32f030xc", "STM32F030xC", flashk= 256, ramk= 32),
        Chip("stm32f031x4", "STM32F031x6", flashk=  16, ramk=  4),
        Chip("stm32f031x6", "STM32F031x6", flashk=  32, ramk=  4),
        #Chip("stm32f070x6", "STM32F070x6", flashk=  32, ramk=  6),
        #Chip("stm32f070xb", "STM32F070xB", flashk= 256, ramk= 32),
        Chip("stm32f103x4", "STM32F103x6", flashk=  16, ramk=  6),
        Chip("stm32f103x6", "STM32F103x6", flashk=  32, ramk= 10),
        Chip("stm32f103x8", "STM32F103xB", flashk=  64, ramk= 20),
        Chip("stm32f103xb", "STM32F103xB", flashk= 128, ramk= 20),

        # stm32f1
        Chip("stm32f103xc", "STM32F103xE", flashk= 256, ramk= 64),
        Chip("stm32f103xd", "STM32F103xE", flashk= 384, ramk= 64),
        Chip("stm32f103xe", "STM32F103xE", flashk= 512, ramk= 64),
        Chip("stm32f103xf", "STM32F103xG", flashk= 768, ramk= 96),
        Chip("stm32f103xg", "STM32F103xG", flashk=1024, ramk= 96),

        # stm32f3
        Chip("stm32f303x6", "STM32F303x8", flashk=  32, ramk= 12, ccmramk= 4),
        Chip("stm32f303x8", "STM32F303x8", flashk=  64, ramk= 12, ccmramk= 4),
        Chip("stm32f303xb", "STM32F303xC", flashk= 128, ramk= 32, ccmramk= 8),
        Chip("stm32f303xc", "STM32F303xC", flashk= 256, ramk= 40, ccmramk= 8),
        Chip("stm32f303xd", "STM32F303xE", flashk= 384, ramk= 64, ccmramk=16),
        Chip("stm32f303xe", "STM32F303xE", flashk= 512, ramk= 64, ccmramk=16),

        # stm32g0
        Chip("stm32g030x6", "STM32G030xx", flashk=  32, ramk=  8),
        Chip("stm32g030x8", "STM32G030xx", flashk=  64, ramk=  8),
        Chip("stm32g031x4", "STM32G031xx", flashk=  16, ramk=  8),
        Chip("stm32g031x6", "STM32G031xx", flashk=  32, ramk=  8),
        Chip("stm32g031x8", "STM32G031xx", flashk=  64, ramk=  8),
        Chip("stm32g0b1xb", "STM32G0B1xx", flashk= 128, ramk=128),
        Chip("stm32g0b1xc", "STM32G0B1xx", flashk= 256, ramk=128),
        Chip("stm32g0b1xe", "STM32G0B1xx", flashk= 512, ramk=128),

        # stm32g4
        Chip("stm32g431x6", "STM32G431xx", flashk=  32, ramk= 16, ccmramk=10),
        Chip("stm32g431x8", "STM32G431xx", flashk=  64, ramk= 16, ccmramk=10),
        Chip("stm32g431xb", "STM32G431xx", flashk= 128, ramk= 16, ccmramk=10),
    ]])

    class MajorFamily:
        def __init__(self, name, arch_name):
            self.name = name
            self.arch = ArmPlatform.ARCHITECTURES[arch_name]
            driver_root = os.path.join(STM32_VENDOR_ROOT, "STM32Cube" + name[-2:], "Drivers")
            self.cmsis_root = os.path.join(driver_root, "CMSIS", "Device", "ST", name+"xx")
            self.hal_root = os.path.join(driver_root, name+"xx_HAL_Driver")
            self.sources = os.path.join(self.cmsis_root, "Source", "Templates", "system_"+name.lower()+"xx.c")

    MAJOR_FAMILIES = dict([(fam.name, fam) for fam in [
        MajorFamily("STM32C0", "m0plus"),
        MajorFamily("STM32F0", "m0"),
        MajorFamily("STM32F1", "m3"),
        MajorFamily("STM32F3", "m4f"),
        MajorFamily("STM32G0", "m0plus"),
        MajorFamily("STM32G4", "m4f"),
    ]])

    def part_name(self):
        return self.chip_name

    def periph_dir(self):
        return os.path.join("arm", "stm32", "periph")

    def cflags(self):
        return self.platform_cflags(self.major_family.arch) + [
            "-DRULOS_ARM_STM32",
            "-DUSE_FULL_LL_DRIVER",
            "-DUSE_HAL_DRIVER",
            "-DRULOS_ARM_"+self.chip.major_family_name.lower(),
            "-D"+self.major_family.name,
            f"-D{self.chip.family}=1",
        ]

    def ld_flags(self, target):
        return self.arm_ld_flags(target)

    def include_dirs(self, target):
        return self.platform_include_dirs(target) + [
            STM32_ROOT,
            os.path.join(self.major_family.cmsis_root, "Include"),
            os.path.join(self.major_family.hal_root, "Inc"),
        ]

    def platform_lib_source_files(self):
        return (self.arm_platform_lib_source_files()
            + util.cglob(STM32_ROOT, "core")
            + util.template_free_cglob(self.major_family.hal_root, "Src")
            + util.cwd_to_project_root([self.major_family.sources]))

    def platform_specific_app_sources(self):
        return util.cwd_to_project_root([
            os.path.join(self.major_family.cmsis_root, "Source", "Templates", "gcc",
                f"startup_{self.chip.family.lower()}.s"),
            ])

    def configure_env(self, env):
        self.arm_configure_env(env)

        linkscript_name = f"{self.chip.name}-linker-script.ld"
        linkscript = env.Command(
            source = os.path.join(env["RulosBuildObjDir"],
                util.cwd_to_project_root(os.path.join(STM32_ROOT, "linker", "stm32-generic.ld"))),
            target = os.path.join(env["RulosBuildObjDir"], "src", "lib", linkscript_name),
            action = env.SpinnerAction(
                f'sed "s/%RULOS_FLASHK%/{self.chip.flashk}/; s/%RULOS_RAMK%/{self.chip.ramk}/; s/%RULOS_CCMRAMK%/{self.chip.ccmramk}/" $SOURCE > $TARGET',
                'Generating linker script', use_source=False))
        env.Append(LINKFLAGS = ["-T", linkscript])
        env.Depends(env["RulosProgramPath"], linkscript)
