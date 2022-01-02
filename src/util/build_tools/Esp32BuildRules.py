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

ESP32_PLATFORM_VERSION = '1.0.6'
KIT_DESCRIPTION_URL = 'https://dl.espressif.com/dl/package_esp32_index.json'


class Esp32Platform(BaseRules.Platform):
    def __init__(self, extra_peripherals = [], extra_cflags = []):
        super().__init__(extra_peripherals, extra_cflags)
        self.kit_root = os.path.join(util.BUILD_ROOT, "compilers", "esp32", ESP32_PLATFORM_VERSION)

        if not os.path.exists(self.kit_root):
            subprocess.call(args=[
                os.path.join(os.path.dirname(__file__), 'FetchArduinoPackage.py'),
                KIT_DESCRIPTION_URL,
                'esp32',
                ESP32_PLATFORM_VERSION,
                'x86_64-pc-linux-gnu',
                self.kit_root,
            ])

        self.tool_root = os.path.join(self.kit_root, "esp32", f"esp32-{ESP32_PLATFORM_VERSION}", "tools")
        assert os.path.exists(self.tool_root)

        self.sdk_root = os.path.join(self.tool_root, "sdk")
        assert os.path.exists(self.sdk_root)

        self.esptool_path = os.path.join(self.kit_root, "esptool_py", "esptool", "esptool.py")

    def name(self):
        return "esp32"

    def target_suffix(self):
        return ".elf"

    def configure_env(self, env):
        self.configure_compiler(env, f"{self.kit_root}/xtensa-esp32-elf-gcc/xtensa-esp32-elf/bin/xtensa-esp32-elf-")
        env.Append(LIBPATH = [
            os.path.join(self.sdk_root, "lib"),
            os.path.join(self.sdk_root, "ld"),
        ])

        # We add --start-group and --end-group to the link command so that the
        # order of -l arguments is not relevant and dependencies are resolved
        # automatically.
        env.Replace(LINKCOM='$LINK -o $TARGET -Wl,--start-group $LINKFLAGS $__RPATH $SOURCES $_LIBDIRFLAGS $_LIBFLAGS -Wl,--end-group')

        arduino_cargocult = "-lgcc -lesp_websocket_client -lwpa2 -ldetection -lesp_https_server -lwps -lhal -lconsole -lpe -lsoc -lsdmmc -lpthread -llog -lesp_http_client -ljson -lmesh -lesp32-camera -lnet80211 -lwpa_supplicant -lc -lmqtt -lcxx -lesp_https_ota -lulp -lefuse -lpp -lmdns -lbt -lwpa -lspiffs -lheap -limage_util -lunity -lrtc -lmbedtls -lface_recognition -lnghttp -ljsmn -lopenssl -lcore -lfatfs -lm -lprotocomm -lsmartconfig -lxtensa-debug-module -ldl -lesp_event -lesp-tls -lfd -lespcoredump -lesp_http_server -lfr -lsmartconfig_ack -lwear_levelling -ltcp_transport -llwip -lphy -lvfs -lcoap -lesp32 -llibsodium -lbootloader_support -ldriver -lcoexist -lasio -lod -lmicro-ecc -lesp_ringbuf -ldetection_cat_face -lapp_update -lespnow -lface_detection -lapp_trace -lnewlib -lbtdm_app -lwifi_provisioning -lfreertos -lfreemodbus -lethernet -lnvs_flash -lspi_flash -lc_nano -lexpat -lfb_gfx -lprotobuf-c -lesp_adc_cal -ltcpip_adapter -lstdc++"
        env.Append(LIBS = arduino_cargocult.split())
        linkscripts = [
                "esp32_out.ld",
                "esp32.project.ld",
                "esp32.rom.ld",
                "esp32.peripherals.ld",
                "esp32.rom.libgcc.ld",
                "esp32.rom.spiram_incompatible_fns.ld",
        ]
        for linkscript in linkscripts:
            env.Append(LINKFLAGS = ["-T", linkscript])

        genpart_path = os.path.join(self.tool_root, "gen_esp32part.py")
        env.Append(BUILDERS = {
            "MakeBin": Builder(
                src_suffix = ".elf",
                suffix = ".bin",
                action = f"python3 {self.esptool_path} --chip esp32 elf2image --flash_mode dio --flash_freq 80m --flash_size 4MB -o $TARGET $SOURCE",
            ),
            "MakePartMap": Builder(
                src_suffix = ".csv",
                suffix = ".bin",
                action = f"python3 {genpart_path} -q $SOURCE $TARGET",
            ),
        })

    def periph_dir(self):
        return os.path.join("esp32", "periph")

    def include_dirs(self):
        return self.common_include_dirs() + [
            os.path.join(util.SRC_ROOT, "lib", "chip", "esp32"),
            os.path.join(self.sdk_root, "include", "esp32"),
            os.path.join(self.sdk_root, "include", "soc"),
            os.path.join(self.sdk_root, "include", "config"),
        ]

    def ld_flags(self, target):
        return self.common_ld_flags(target) + [
            '-nostdlib',
        ]

    def cflags(self):
        return self.common_cflags() + [
            "-DRULOS_ESP32",
            "-Os",
            "-g3",
            "-Wpointer-arith",
            "-fexceptions",
            "-fstack-protector",
            "-ffunction-sections",
            "-fdata-sections",
            "-fstrict-volatile-bitfields",
            "-mlongcalls",
            "-nostdlib",
            "-gdwarf-2",
            "-std=gnu99",
            "-Os",
        ]

    def platform_lib_source_files(self):
        return util.cglob(util.SRC_ROOT, "lib", "chip", "esp32", "core")

    def platform_specific_app_sources(self):
        return []

    def programming_cmdline(self, usbpath, binfile, partfile):
        return [
            "python3",
            self.esptool_path,
            "--chip", "esp32",
            "--port", usbpath,
            "--baud", "921600",
            "--before", "default_reset",
            "--after", "hard_reset",
            "write_flash",
            "-z",
            "--flash_mode", "dio",
            "--flash_freq", "80m",
            "--flash_size", "detect",
            "0xe000",
            f"{self.tool_root}/partitions/boot_app0.bin",
            "0x1000",
            f"{self.tool_root}/sdk/bin/bootloader_dio_80m.bin",
            "0x10000",
            binfile.get_abspath(),
            "0x8000",
            partfile.get_abspath(),
        ]

    def find_esp32_port(self):
        import serial.tools.list_ports as list_ports

        esp32_ports = list(list_ports.grep('CP2104'))

        if len(esp32_ports) == 0:
            print("No CP2104 USB ports found; is your ESP32 plugged in?")
            return None

        if len(esp32_ports) > 1:
            print("Multiple CP2104 USB ports found. Unplug some, or specify with RULOS_ESP32_PORT env var")
            return None

        return esp32_ports[0].device

    def program_esp32(self, target, source, env):
        binfile = source[0]
        partfile = source[1]
        print("Programming ESP32...")

        if 'RULOS_ESP32_PORT' in os.environ:
            usbpath = os.environ['RULOS_ESP32_PORT']
        else:
            usbpath = self.find_esp32_port()

        if not usbpath:
            return

        cmdline = self.programming_cmdline(usbpath, binfile, partfile)
        print(cmdline)
        subprocess.call(cmdline)

    def post_configure(self, env, outputs):
        binfile = env.MakeBin(outputs[0])
        partfile = env.MakePartMap(
            source=os.path.join(self.tool_root, "partitions", "default.csv"),
            target=f"{outputs[0][0].get_abspath()}.partitions.bin",
        )
        Default([binfile, partfile])

        env.Command("program", [binfile, partfile] + outputs, self.program_esp32)
