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

from BaseRules import *
import glob
import os
import SpecialRules
import subprocess
from filelock import FileLock, Timeout
from SCons.Script import *

sconsign = f".sconsign.{SCons.__version__}.dblite"
lock = FileLock(os.path.join(BUILD_ROOT, sconsign+".lock"))
try:
    lock.acquire(timeout=0.1)
except:
    print("Scons already running against this sconsign file; concurrency will confuse it.")
    sys.exit(-1)
SConsignFile(os.path.join(BUILD_ROOT, sconsign))

def die(msg):
    sys.stderr.write(msg+"\n")
    sys.exit(1)

def cglob(*kargs):
    globpath = tuple(list(kargs) + ["*.c"])
    return cwd_to_project_root(glob.glob(os.path.join(*globpath)))

def template_free_cglob(*kargs):
    return [f for f in cglob(*kargs) if not f.endswith("_template.c")]

def parse_peripherals(peripherals):
    if type(peripherals)==type(""):
        return peripherals.split()
    assert(type(peripherals)==type([]))
    return peripherals

def configure_compiler(env, compiler_prefix):
    env.Replace(CC = compiler_prefix+"gcc")
    env.Replace(AS = compiler_prefix+"as")
    env.Replace(AR = compiler_prefix+"ar")
    env.Replace(RANLIB = compiler_prefix+"ranlib")

    lss_builder = Builder(
        src_suffix = ".elf",
        suffix = ".lss",
        action = compiler_prefix+"objdump -h -S --syms $SOURCE > $TARGET"
        )
    env.Append(BUILDERS = {"MakeLSS": lss_builder})

def pkgconfig(mode, package):
    return subprocess.check_output(["pkg-config", mode, package]).decode("utf-8").split()
        
class Platform:
    def __init__(self, extra_peripherals, extra_cflags):
        self.extra_peripherals = parse_peripherals(extra_peripherals)
        self.extra_cflags = extra_cflags

    def common_libs(self, target):
        src_dirs = []
        src_dirs.append(os.path.join(SRC_ROOT, "lib", "core"))
        for periph_name in target.peripherals + self.extra_peripherals:
            periph_common_dir = os.path.join(SRC_ROOT, "lib", "periph", periph_name)
            periph_platform_dir = os.path.join(SRC_ROOT, "lib", "chip", self.periph_dir(), periph_name)
            if (not os.path.exists(periph_common_dir) and not os.path.exists(periph_platform_dir)):
                die(f"Periph {periph_name} has no source paths (looked in {periph_common_dir}, {periph_platform_dir})")
            src_dirs.append(periph_common_dir)
            src_dirs.append(periph_platform_dir)
        src_files = sum([cglob(d) for d in src_dirs], [])

        src_files.extend(self.platform_lib_source_files())
        return src_files

    def common_cflags(self):
        return self.extra_cflags + [
            "-Wall",
            "-Werror",
            "-g",
            #, "-flto"
        ]

    def common_include_dirs(self):
        return [
            os.path.join(SRC_ROOT, "lib"),
            ".", # app source code directory
        ]

    def map_ld_flag(self, build_obj_dir, target):
        map_path = os.path.join(build_obj_dir,
            f"{target.name}.map")
        return f"-Wl,-Map={map_path},--cref"

    def common_ld_flags(self, target):
        return [
            "-Wl,--fatal-warnings", # linker should fail on warning
            "-Wl,--gc-sections",    # garbage-collect unused functions
        ]

    def build(self, target):
        env = Environment()

        # Export names & paths to subclasses
        build_obj_dir = os.path.join(BUILD_ROOT, target.name, self.name())
        env.Replace(RulosBuildObjDir = build_obj_dir)

        program_name = os.path.join(build_obj_dir, target.name+".elf")
        env.Replace(RulosProgramName = program_name)

        env.Replace(RulosProjectRoot = PROJECT_ROOT)    # export PROJECT_ROOT to converter actions

        self.configure_env(env)
    
        platform_lib_srcs = [os.path.join(build_obj_dir, s) for s in self.common_libs(target)]
        env.Append(CFLAGS = self.cflags())
        env.Append(CFLAGS = target.cflags())
        env.Append(LINKFLAGS = self.cflags())
        env.Append(LINKFLAGS = target.cflags())
        env.Append(LINKFLAGS = self.ld_flags(target))
        env.Append(LINKFLAGS = self.map_ld_flag(build_obj_dir, target))
        env.Append(CPPPATH = self.include_dirs())
        env.Append(CPPPATH = os.path.join(build_obj_dir, "src"))

        # We're pretending the sources appear in build_obj_dir, so they look adjacent to the build output.
        env.VariantDir(build_obj_dir, PROJECT_ROOT, duplicate=0)

        # Build the lib
        rocket_lib = env.StaticLibrary(os.path.join(build_obj_dir, "src", "lib", "rocket"),
            source=platform_lib_srcs)
        for converter in SpecialRules.CONVERTERS:
            converter_output = env.Command(
                source = [os.path.join(build_obj_dir, p) for p in converter.script_input],
                target = os.path.join(build_obj_dir, converter.intermediate_file),
                action = converter.action)
            env.Depends(
                os.path.join(build_obj_dir, converter.dependent_source),
                os.path.join(build_obj_dir, converter.intermediate_file))

        # Build the app
        target_sources = [
            os.path.join(build_obj_dir, s) for s in target.sources + self.platform_specific_app_sources()]

# Our failed attempt to tell scons we actually *want* the .map file that's a side effect of
# building the app_binary with self.map_ld_flag.
#        def elf_emitter(target, source, env):
#            print(target[0])
#        env["BUILDERS"]["Program"].add_emitter(suffix = ".elf", emitter = elf_emitter)
        app_binary = env.Program(env["RulosProgramName"], source=target_sources + rocket_lib)

        self.post_configure(env, app_binary)
        Default([app_binary])

# TODO move knowledge to the platform-specific directories
class ArmPlatform(Platform):
    def __init__(self, chip_name, extra_peripherals, extra_cflags):
        super().__init__(extra_peripherals, extra_cflags)
        self.chip_name = chip_name

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
            "-std=gnu99",
            "-O2",
#            "-D__STACK_SIZE=$(STACK_SIZE)",
#            "-D__HEAP_SIZE=$(HEAP_SIZE)",
            ]

    def arm_ld_flags(self, target):
        return self.common_ld_flags(target) + [
            "-nostartfiles",
            "-static",
            ]

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

    def arm_configure_env(self, env):
        configure_compiler(env, "/usr/local/bin/gcc-arm-none-eabi-9-2019-q4-major/bin/arm-none-eabi-")

    def post_configure(self, env, app_binary):
        # Returns list of extra Default targets
        Default(env.MakeLSS(app_binary))

STM32_ROOT = os.path.join(ArmPlatform.ARM_ROOT, "stm32")
class ArmStmPlatform(ArmPlatform):
    def __init__(self, chip_name, extra_peripherals = [], extra_cflags = []):
        super().__init__(chip_name, extra_peripherals, extra_cflags)
        if (chip_name not in self.CHIPS):
            die(f"Unrecognized chip {chip_name}")
        self.chip = self.CHIPS[self.chip_name]
        self.major_family = self.MAJOR_FAMILIES[self.chip.major_family_name]

    class Chip:
        def __init__(self, name, family, flashk, ramk):
            self.name = name
            self.family = family
            self.flashk = flashk
            self.ramk = ramk
            self.major_family_name = family[:7]

    CHIPS = dict([(chip.name, chip) for chip in [
        Chip("stm32f030x4", "STM32F030x6", flashk=  16, ramk= 4),
        Chip("stm32f030x6", "STM32F030x6", flashk=  32, ramk= 4),
        Chip("stm32f030x8", "STM32F030x8", flashk=  64, ramk= 8),
        Chip("stm32f030xc", "STM32F030xC", flashk= 256, ramk=32),
        Chip("stm32f031x4", "STM32F031x6", flashk=  16, ramk= 4),
        Chip("stm32f031x6", "STM32F031x6", flashk=  32, ramk= 4),
        #Chip("stm32f070x6", "STM32F070x6", flashk=  32, ramk= 6),
        #Chip("stm32f070xb", "STM32F070xB", flashk= 256, ramk=32),
        Chip("stm32f103x4", "STM32F103x6", flashk=  16, ramk= 6),
        Chip("stm32f103x6", "STM32F103x6", flashk=  32, ramk=10),
        Chip("stm32f103x8", "STM32F103xB", flashk=  64, ramk=20),
        Chip("stm32f103xb", "STM32F103xB", flashk= 128, ramk=20),
        Chip("stm32f103xc", "STM32F103xE", flashk= 256, ramk=64),
        Chip("stm32f103xd", "STM32F103xE", flashk= 384, ramk=64),
        Chip("stm32f103xe", "STM32F103xE", flashk= 512, ramk=64),
        Chip("stm32f103xf", "STM32F103xG", flashk= 768, ramk=96),
        Chip("stm32f103xg", "STM32F103xG", flashk=1024, ramk=96),
        Chip("stm32f303x6", "STM32F303x8", flashk=  32, ramk=12),
        Chip("stm32f303x8", "STM32F303x8", flashk=  64, ramk=12),
        Chip("stm32f303xb", "STM32F303xC", flashk= 128, ramk=32),
        Chip("stm32f303xc", "STM32F303xC", flashk= 256, ramk=40),
        Chip("stm32f303xd", "STM32F303xD", flashk= 384, ramk=64),
        Chip("stm32f303xd", "STM32F303xD", flashk= 512, ramk=64),
        Chip("stm32g030x6", "STM32G030xx", flashk=  32, ramk= 8),
        Chip("stm32g030x8", "STM32G030xx", flashk=  64, ramk= 8),
        Chip("stm32g031x4", "STM32G031xx", flashk=  16, ramk= 8),
        Chip("stm32g031x6", "STM32G031xx", flashk=  32, ramk= 8),
        Chip("stm32g031x8", "STM32G031xx", flashk=  64, ramk= 8),
        Chip("stm32g431x6", "STM32G431xx", flashk=  32, ramk=16),
        Chip("stm32g431x8", "STM32G431xx", flashk=  64, ramk=16),
        Chip("stm32g431xb", "STM32G431xx", flashk= 128, ramk=16),
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
            f"-D{self.chip.family}=1",
            ]

    def ld_flags(self, target):
        return self.arm_ld_flags(target)

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
            + cwd_to_project_root([self.major_family.sources]))

    def platform_specific_app_sources(self):
        return cwd_to_project_root([
            os.path.join(self.major_family.cmsis_root, "Source", "Templates", "gcc",
                f"startup_{self.chip.family.lower()}.s"),
            ])

    def configure_env(self, env):
        self.arm_configure_env(env)

        linkscript_name = f"{self.chip.name}-linker-script.ld"
        linkscript = env.Command(
            source = os.path.join(env["RulosBuildObjDir"],
                cwd_to_project_root(os.path.join(STM32_ROOT, "linker", "stm32-generic.ld"))),
            target = os.path.join(env["RulosBuildObjDir"], "src", "lib", linkscript_name),
            action = f'sed "s/%RULOS_FLASHK%/{self.chip.flashk}/; s/%RULOS_RAMK%/{self.chip.ramk}/" $SOURCE > $TARGET')
        env.Append(LINKFLAGS = ["-T", linkscript])
        env.Depends(env["RulosProgramName"], linkscript)

class AvrPlatform(Platform):
    def __init__(self, mcu, extra_peripherals = [], extra_cflags = []):
        super().__init__(extra_peripherals, extra_cflags)
        self.mcu = mcu

    def name(self):
        return f"avr-{self.mcu}"

    def periph_dir(self):
        return os.path.join("avr", "periph")

    def configure_env(self, env):
        configure_compiler(env, "avr-")

    def platform_lib_source_files(self):
        return cglob(SRC_ROOT, "lib", "chip", "avr", "core")

    def cflags(self):
        return self.common_cflags() + [
            "-mmcu="+self.mcu,
            "-DRULOS_AVR", 
            "-funsigned-char",
            "-funsigned-bitfields",
            "-fpack-struct",
            "-fshort-enums",
            "-fdata-sections",
            "-ffunction-sections", # Put all funcs/data in their own sections
            "-gdwarf-2",
            "-std=gnu99",
            "-Os",
            f"-DMCU{self.mcu}=1",
            ]

    def include_dirs(self):
        return self.common_include_dirs() + [ os.path.join(SRC_ROOT, "lib", "chip", "avr") ]

    def ld_flags(self, target):
        return self.common_ld_flags(target)

    def platform_specific_app_sources(self):
        return []

    def post_configure(self, env, app_binary):
        HEX_FLASH_FLAGS = "-R .eeprom -R .fuse -R .lock -R .signature"
        env.Append(BUILDERS = {"MakeHex": Builder(
            src_suffix = ".elf",
            suffix = ".hex",
            action = f"avr-objcopy -O ihex {HEX_FLASH_FLAGS}  $SOURCE $TARGET")})
        Default(env.MakeHex(app_binary))

        Default(env.MakeLSS(app_binary))

class SimulatorPlatform(Platform):
    def __init__(self, extra_peripherals = [], extra_cflags = []):
        super().__init__(extra_peripherals, extra_cflags)

    def name(self):
        return "simulator"

    def configure_env(self, env):
        env.Append(LIBS = ["m", "ncurses"] + pkgconfig("--libs", "gtk+-2.0"))

    def periph_dir(self):
        return os.path.join("sim", "periph")

    def platform_lib_source_files(self):
        return cglob(SRC_ROOT, "lib", "chip", "sim", "core")

    def cflags(self):
        return self.common_cflags() + pkgconfig("--cflags", "gtk+-2.0") + [ "-DSIM" ]

    def ld_flags(self, target):
        return  self.common_ld_flags(target)

    def include_dirs(self):
        return self.common_include_dirs() + [ os.path.join(SRC_ROOT, "lib", "chip", "sim") ]

    def platform_specific_app_sources(self):
        return []

    def post_configure(self, env, app_binary):
        pass

class RulosBuildTarget:
    def __init__(self, name, sources, platforms, peripherals = [], extra_cflags = []):
        self.name = name
        self.sources = [os.path.relpath(s, PROJECT_ROOT) for s in sources]
        self.platforms = platforms
        assert(type(self.platforms)==type([]))
        for p in self.platforms:
            assert(isinstance(p, Platform))
        self.peripherals = parse_peripherals(peripherals)
        self.extra_cflags = extra_cflags

    def build(self):
        for platform in self.platforms:
            platform.build(self)

    def cflags(self):
        return self.extra_cflags
