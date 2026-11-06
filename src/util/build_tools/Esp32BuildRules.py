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

# RULOS on Espressif chips is built by ESP-IDF. SCons decides what to
# build -- the target, its sources, flags and peripherals -- and writes
# that out as a small ESP-IDF project (RULOS as one component, the app as
# another), then hands the compile to idf.py. IDF builds itself, the
# bootloader and the partition table from the project's sdkconfig, so each
# app controls the full configuration. The IDF checkout and its toolchains
# live under build/compilers/esp-idf and are fetched on first use.

import os
import shlex
import subprocess
import sys
from . import BaseRules, Lvgl, SpecialRules, util
from SCons.Script import *

ESP_IDF_VERSION = "v5.5.6"
ESP_IDF_REPO = "https://github.com/espressif/esp-idf.git"

# Chips RULOS has glue for. Toolchains for all of them are installed
# together.
CHIPS = ["esp32", "esp32s3"]

# Build tools IDF needs from the host; on Linux its installer leaves them to
# the distribution, so they are fetched explicitly to keep the build
# self-contained.
HOST_TOOLS = ["cmake", "ninja"]

# IDF components whose headers RULOS' ESP32 code includes, by RULOS
# peripheral. "core" covers src/lib/chip/esp32/core.
IDF_COMPONENTS = {
    "core": ["esp_driver_gpio", "esp_driver_gptimer", "nvs_flash"],
    "uart": ["esp_driver_uart", "esp_driver_usb_serial_jtag"],
    "inet": ["esp_wifi", "esp_netif", "esp_event", "esp_http_client"],
    "ntp": ["esp_wifi", "lwip"],
    "lvgl": ["lvgl__lvgl", "esp_lcd"],
    "ch422g": ["esp_driver_i2c"],
    "sprite_show": ["lvgl__lvgl", "esp_partition"],
    "lcd_touch": ["lvgl__lvgl", "espressif__esp_lcd_touch"],
}

# Compiler options for individual ESP-IDF components. IDF compiles
# everything with -Werror=all; these quiet warnings in code we do not own.
IDF_COMPONENT_COMPILE_OPTIONS = {
    # LVGL 9.3's binary decoder logs a bit-field with the wrong format
    "lvgl__lvgl": ["-Wno-error=format"],
}

# Component-registry dependencies a RULOS peripheral brings in, as
# {name: version}. The registry names them <namespace>__<name> inside the
# build, which is how IDF_COMPONENTS refers to them.
IDF_REGISTRY_COMPONENTS = {
    "lvgl": {"lvgl/lvgl": Lvgl.VERSION},
    "lcd_touch": {"espressif/esp_lcd_touch": "^1"},
}

# sdkconfig every RULOS target starts from; the SConstruct's sdkconfig
# argument layers on top. Keys omit the CONFIG_ prefix.
SDKCONFIG_DEFAULTS = {
    # hal_idle() yields for one tick, so the tick is the scheduler's
    # wakeup granularity
    "FREERTOS_HZ": 1000,
    "COMPILER_OPTIMIZATION_SIZE": True,
    "ESPTOOLPY_FLASHFREQ_80M": True,
    # a single 1.5 MB app slot; RULOS does not use OTA
    "PARTITION_TABLE_SINGLE_APP_LARGE": True,
}

# where a board's partition table is written, relative to the project
PARTITION_TABLE_FILENAME = "partitions.csv"

# USB serial adapters an ESP32 board might show up as: Silicon Labs and
# WCH bridges, plus the USB-Serial/JTAG peripheral built into newer chips
# (Espressif VID 303A, PID 1001).
USB_SERIAL_PORT_REGEX = r"CP210|CH34|303A:1001|USB Single"

# Serial rate for the run/monitor aliases; RULOS_ESP32_BAUD overrides it.
DEFAULT_MONITOR_BAUD = "1000000"


class EspIdf:
    """The ESP-IDF checkout and tools under build/compilers/esp-idf, fetched
    on first use. One instance per SCons run; see get()."""

    _instance = None

    @classmethod
    def get(cls):
        if cls._instance is None:
            cls._instance = EspIdf()
        return cls._instance

    def __init__(self):
        root = os.path.abspath(os.path.join(util.BUILD_ROOT, "compilers", "esp-idf"))
        self.path = os.path.join(root, ESP_IDF_VERSION)
        self.tools_path = os.path.join(root, "tools")
        self.component_cache = os.path.join(root, "component-cache")
        self.idf_tools = [sys.executable, os.path.join(self.path, "tools", "idf_tools.py")]

        if not os.path.exists(self.path):
            print(f"Cloning ESP-IDF {ESP_IDF_VERSION} into {self.path}")
            subprocess.check_call([
                "git",
                "clone",
                "--depth",
                "1",
                "--branch",
                ESP_IDF_VERSION,
                "--recurse-submodules",
                "--shallow-submodules",
                ESP_IDF_REPO,
                self.path,
            ])

        self.env = self._export()
        if self.env is None:
            self._install()
            self.env = self._export()
            assert self.env is not None, "ESP-IDF tool install did not produce a usable environment"

    def _base_env(self):
        env = dict(os.environ)
        env["IDF_PATH"] = self.path
        env["IDF_TOOLS_PATH"] = self.tools_path
        return env

    def _install(self):
        print(f"Installing ESP-IDF toolchains into {self.tools_path}")
        subprocess.check_call(
            self.idf_tools + ["install", "--targets", ",".join(CHIPS), "required"] + HOST_TOOLS,
            env=self._base_env(),
        )
        subprocess.check_call(self.idf_tools + ["install-python-env"], env=self._base_env())

    # The environment export.sh would set up, or None if the tools are not
    # installed. Captured once so builds do not pay for a shell each.
    def _export(self):
        result = subprocess.run(
            self.idf_tools + ["export", "--format", "key-value"],
            env=self._base_env(),
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            return None
        env = self._base_env()
        for line in result.stdout.splitlines():
            key, _, value = line.partition("=")
            if key == "PATH":
                value = value + os.pathsep + env["PATH"]
            env[key] = value
        if "IDF_PYTHON_ENV_PATH" not in env or not os.path.exists(env["IDF_PYTHON_ENV_PATH"]):
            return None
        env["IDF_COMPONENT_CACHE_PATH"] = self.component_cache
        if util.which("ccache"):
            env["IDF_CCACHE_ENABLE"] = "1"
        return env

    def idf_py(self):
        return [
            os.path.join(self.env["IDF_PYTHON_ENV_PATH"], "bin", "python"),
            os.path.join(self.path, "tools", "idf.py"),
        ]


def _sdkconfig_line(key, value):
    if value is True:
        value = "y"
    elif value is False:
        value = "n"
    elif isinstance(value, str):
        value = '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'
    return f"CONFIG_{key}={value}"


# Quotes one argument for a CMake command so the compiler receives it
# verbatim.
def _cmake_quote(arg):
    escaped = arg.replace("\\", "\\\\").replace('"', '\\"').replace(";", "\;").replace("$", "\\$")
    return f'"{escaped}"'


def _cmake_list(args):
    return " ".join(_cmake_quote(a) for a in args)


# Returns whether the file was written.
def _write_if_changed(path, content):
    if os.path.exists(path) and open(path).read() == content:
        return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(content)
    return True


class Esp32Platform(BaseRules.Platform):
    # chip: one of CHIPS; "esp32" when neither it nor a board is given.
    # board: an Esp32Boards.Esp32Board, which supplies the chip and the
    #     sdkconfig describing the module.
    # sdkconfig: ESP-IDF configuration layered over SDKCONFIG_DEFAULTS and
    #     the board's, as {key-without-CONFIG_: value}. Values may be bool,
    #     int or str.
    # idf_components: ESP-IDF component registry dependencies for the app,
    #     as {name: version spec}, e.g. {"espressif/esp_lcd_touch_gt911": "^1"}.
    #     Peripherals add their own (IDF_REGISTRY_COMPONENTS).
    # lv_conf: the app's lv_conf.h, for the lvgl peripheral.
    # partition_table: the flash layout in ESP-IDF's CSV form, replacing the
    #     board's.
    # flash_partitions: files to flash into partitions of that layout along
    #     with the app, as {partition name: path}.
    def __init__(
        self,
        chip=None,
        board=None,
        sdkconfig={},
        idf_components={},
        lv_conf=None,
        partition_table=None,
        flash_partitions={},
        extra_peripherals=[],
        extra_cflags=[],
    ):
        super().__init__(extra_peripherals, extra_cflags)
        if board is not None:
            assert chip is None or chip == board.chip, f"{board.name} is an {board.chip}"
            chip = board.chip
        if chip is None:
            chip = "esp32"
        assert chip in CHIPS, f"unknown ESP32 chip {chip}; known: {CHIPS}"
        self.chip = chip
        self.sdkconfig = dict(SDKCONFIG_DEFAULTS)
        if board is not None:
            self.sdkconfig.update(board.sdkconfig)
            if partition_table is None:
                partition_table = board.partition_table
        self.partition_table = partition_table
        if partition_table is not None:
            self.sdkconfig["PARTITION_TABLE_CUSTOM"] = True
            self.sdkconfig["PARTITION_TABLE_SINGLE_APP_LARGE"] = False
            self.sdkconfig["PARTITION_TABLE_CUSTOM_FILENAME"] = PARTITION_TABLE_FILENAME
        self.flash_partitions = {
            name: os.path.abspath(path) for name, path in flash_partitions.items()
        }
        assert (
            not self.flash_partitions or partition_table is not None
        ), "flash_partitions needs a partition_table naming them"
        self.sdkconfig.update(sdkconfig)
        self.idf_components = dict(idf_components)
        self.lv_conf = None if lv_conf is None else os.path.abspath(lv_conf)
        # Everything compiles as C++ on this platform: the chip glue is
        # written against IDF's C++-visible API, and RULOS' C headers carry
        # no extern "C" guards for C++ callers.
        self.use_cpp = True
        # ESP-IDF does not support LTO
        self.use_lto = False
        self.env_to_ifdefs.extend([
            "RULOS_NTP_SERVER",
        ])
        self.idf = EspIdf.get()

    def name(self):
        return self.chip

    def target_suffix(self):
        return ".elf"

    def periph_dir(self):
        return os.path.join("esp32", "periph")

    def platform_lib_source_files(self):
        return util.cglob(util.SRC_ROOT, "lib", "chip", "esp32", "core")

    def cflags(self):
        return self.common_cflags() + [
            "-DRULOS_ESP32",
            f"-DRULOS_{self.chip.upper()}",
            # IDF compiles with -Wextra; RULOS' warning policy is -Wall
            # -Werror on every platform
            "-Wno-extra",
        ]

    def _peripherals(self, target):
        return target.peripherals + self.extra_peripherals

    def _idf_components(self, target):
        components = set(IDF_COMPONENTS["core"])
        for periph in self._peripherals(target):
            components.update(IDF_COMPONENTS.get(periph, []))
        return sorted(components)

    def _registry_components(self, target):
        components = {}
        for periph in self._peripherals(target):
            components.update(IDF_REGISTRY_COMPONENTS.get(periph, {}))
        components.update(self.idf_components)
        return components

    # Preprocessor definitions for every component in the project, the
    # SDK's included.
    def _project_definitions(self, target):
        definitions = {}
        if "lvgl" in self._peripherals(target):
            assert self.lv_conf is not None, "the lvgl peripheral needs Esp32Platform(lv_conf=...)"
            # LVGL reads its configuration from this path instead of the
            # sdkconfig, so the device and host builds share one file.
            definitions["LV_CONF_PATH"] = f'"{self.lv_conf}"'
        return definitions

    # The compiler flags, as the compiler should see them. RULOS flag lists
    # are written for a shell command line, so shell quoting is undone
    # here; CMake passes options verbatim.
    def _compiler_args(self, target):
        return [arg for flag in self.all_cflags(target) for arg in shlex.split(flag)]

    def _abs_sources(self, relpaths):
        return [os.path.abspath(os.path.join(util.PROJECT_ROOT, p)) for p in relpaths]

    def _component_cmake(self, sources, include_dirs, requires, compiler_args):
        # C and C++ sources are all compiled as C++; assembly stays assembly
        c_sources = [s for s in sources if not s.endswith(".S")]
        return f"""# generated by RULOS; see src/util/build_tools/Esp32BuildRules.py
cmake_policy(SET CMP0119 NEW)
idf_component_register(
    SRCS {_cmake_list(sources)}
    INCLUDE_DIRS {_cmake_list(include_dirs)}
    REQUIRES {" ".join(requires)}
)
set_source_files_properties({_cmake_list(c_sources)} PROPERTIES LANGUAGE CXX)
target_compile_options(${{COMPONENT_LIB}} PRIVATE {_cmake_list(compiler_args)})
"""

    def _write_project(self, project_dir, program, target):
        lib_sources = self._abs_sources(self.common_libs(target))
        app_sources = self._abs_sources(target.sources)
        for converter in SpecialRules.CONVERTERS + target.extra_converters:
            dependent = os.path.abspath(os.path.join(util.PROJECT_ROOT, converter.dependent_source))
            if dependent in lib_sources + app_sources:
                util.die(
                    f"{converter.dependent_source} needs a generated source; "
                    "the ESP-IDF build does not run converters"
                )

        include_dirs = [os.path.abspath(d) for d in self.common_include_dirs(target)] + [
            os.path.abspath(os.path.join(util.SRC_ROOT, "lib", "chip", "esp32")),
        ]
        compiler_args = self._compiler_args(target)

        sdkconfig = dict(self.sdkconfig)
        sdkconfig["IDF_TARGET"] = self.chip
        project_options = "".join(
            f"idf_build_set_property(COMPILE_OPTIONS {_cmake_quote(f'-D{k}={v}')} APPEND)\n"
            for k, v in sorted(self._project_definitions(target).items())
        )
        component_options = "".join(
            f"idf_component_get_property(target_{i} {name} COMPONENT_LIB)\n"
            f"target_compile_options(${{target_{i}}} PRIVATE {_cmake_list(options)})\n"
            for i, (name, options) in enumerate(sorted(IDF_COMPONENT_COMPILE_OPTIONS.items()))
            if name in self._idf_components(target)
        )
        # data partitions flashed along with the app
        flash_partitions = "".join(
            f"esptool_py_flash_to_partition(flash {_cmake_quote(name)} {_cmake_quote(path)})\n"
            for name, path in sorted(self.flash_partitions.items())
        )
        files = {
            "CMakeLists.txt": (
                f"""# generated by RULOS; see src/util/build_tools/Esp32BuildRules.py
cmake_minimum_required(VERSION 3.16)
include($ENV{{IDF_PATH}}/tools/cmake/project.cmake)
# build only the components the app's dependencies reach
idf_build_set_property(MINIMAL_BUILD ON)
{project_options}project({program})
{component_options}{flash_partitions}"""
            ),
            "sdkconfig.defaults": "".join(
                _sdkconfig_line(k, v) + "\n" for k, v in sorted(sdkconfig.items())
            ),
            os.path.join("components", "rulos", "CMakeLists.txt"): self._component_cmake(
                lib_sources, include_dirs, self._idf_components(target), compiler_args
            ),
            os.path.join("main", "CMakeLists.txt"): self._component_cmake(
                app_sources, [], ["rulos"], compiler_args
            ),
        }
        if self.partition_table is not None:
            files[PARTITION_TABLE_FILENAME] = self.partition_table
        registry_components = self._registry_components(target)
        if registry_components:
            files[os.path.join("main", "idf_component.yml")] = "dependencies:\n" + "".join(
                f'  {name}: "{spec}"\n' for name, spec in sorted(registry_components.items())
            )
        for relpath, content in files.items():
            changed = _write_if_changed(os.path.join(project_dir, relpath), content)
            # IDF reads sdkconfig.defaults only when it creates sdkconfig,
            # so a changed configuration must start over from the defaults.
            if changed and relpath == "sdkconfig.defaults":
                sdkconfig_path = os.path.join(project_dir, "sdkconfig")
                if os.path.exists(sdkconfig_path):
                    os.remove(sdkconfig_path)

    def _idf_cmdline(self, env, args):
        return (
            self.idf.idf_py()
            + [
                "-C",
                env["RulosIdfProjectDir"],
                "-B",
                env["RulosBuildObjDir"],
            ]
            + args
        )

    # Runs idf.py. Its output is held back and shown only on failure, unless
    # SCons is in --verbose mode or the command is interactive.
    def _run_idf(self, env, args, interactive=False):
        cmdline = self._idf_cmdline(env, args)
        quiet = not interactive and not GetOption("verbose")
        if not quiet:
            print(" ".join(shlex.quote(c) for c in cmdline))
        result = subprocess.run(cmdline, env=env["ENV"], capture_output=quiet, text=quiet)
        if result.returncode != 0 and quiet:
            sys.stdout.write(result.stdout)
            sys.stderr.write(result.stderr)
        return result.returncode

    def _idf_build(self, target, source, env):
        return self._run_idf(env, ["build"])

    def _port_args(self):
        if "RULOS_ESP32_PORT" in os.environ:
            return ["-p", os.environ["RULOS_ESP32_PORT"]]
        # idf.py probes the serial ports itself when none is given
        return []

    def _monitor_baud(self):
        return os.environ.get("RULOS_ESP32_BAUD", DEFAULT_MONITOR_BAUD)

    def _idf_flash(self, target, source, env):
        return self._run_idf(env, self._port_args() + ["flash"], interactive=True)

    def _idf_monitor(self, target, source, env):
        return self._run_idf(
            env, self._port_args() + ["monitor", "-b", self._monitor_baud()], interactive=True
        )

    def find_esp32_port(self):
        if "RULOS_ESP32_PORT" in os.environ:
            return os.environ["RULOS_ESP32_PORT"]

        import serial.tools.list_ports as list_ports

        print("Port list:")
        for port in list_ports.comports():
            print(f"port {port}")
        esp32_ports = list(list_ports.grep(USB_SERIAL_PORT_REGEX))

        if len(esp32_ports) == 0:
            raise Exception("No ESP32-looking USB serial ports found; is your board plugged in?")

        if len(esp32_ports) > 1:
            raise Exception(
                "Multiple ESP32-looking USB serial ports found. Unplug some, or specify with RULOS_ESP32_PORT env var"
            )

        return esp32_ports[0].device

    def run_esp32(self, target, source, env):
        usbpath = self.find_esp32_port()
        subprocess.call(
            ["grabserial", "-t", "-d", usbpath, "-b", self._monitor_baud(), "-o", "esp32.log"]
        )

    def run_esp32_epochsec(self, target, source, env):
        usbpath = self.find_esp32_port()
        subprocess.call([
            "grabserial",
            "-T",
            "-F",
            "%s.%f",
            "-d",
            usbpath,
            "-b",
            self._monitor_baud(),
            "-o",
            "esp32.log",
        ])

    def build(self, target):
        build_dir = os.path.abspath(self.build_obj_dir(target))
        project_dir = os.path.join(build_dir, "project")
        program = self.program_name(target)
        self._write_project(project_dir, program, target)

        env = self.make_env()
        env["ENV"] = dict(self.idf.env)
        env["ENV"]["IDF_TARGET"] = self.chip
        env.Replace(RulosBuildObjDir=build_dir)
        env.Replace(RulosIdfProjectDir=project_dir)

        # Ninja tracks the sources, so the build always runs and is a no-op
        # when nothing changed.
        elf = os.path.join(build_dir, program + self.target_suffix())
        app = env.Command(
            target=elf,
            source=[],
            action=env.SpinnerAction(self._idf_build, "Building with ESP-IDF", use_source=False),
        )
        env.AlwaysBuild(app)
        env.Clean(app, build_dir)
        Default(app)

        # Create programming aliases so a command like "scons
        # program-esp32-<progname>" works, even for SConstruct files
        # that have multiple targets
        prog_target = f"{self.name()}-{target.name}"
        program_alias = env.Alias(f"program-{prog_target}", app, self._idf_flash)
        env.Alias(f"run-{prog_target}", program_alias, self.run_esp32)
        env.Alias(f"rune-{prog_target}", program_alias, self.run_esp32_epochsec)
        env.Alias(f"monitor-{prog_target}", program_alias, self._idf_monitor)

        # Simpler "scons program" and "scons run" commands for simple
        # SConstruct files that have only a single target
        for verb in ["program", "run", "rune", "monitor"]:
            env.Alias(verb, f"{verb}-{prog_target}")
