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

ESP32_PLATFORM_VERSION = '2.0.2'
KIT_DESCRIPTION_URL = 'https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json'


class Esp32Platform(BaseRules.Platform):
    def __init__(self, extra_peripherals = [], extra_cflags = []):
        super().__init__(extra_peripherals, extra_cflags)
        self.use_cpp = True
        self.env_to_ifdefs.extend([
            "RULOS_NTP_SERVER",
        ])
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

        self.sdk_root = os.path.join(self.tool_root, "sdk", "esp32")
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

        arduino_cargocult = "-lesp_ringbuf -lefuse -lesp_ipc -ldriver -lesp_pm -lmbedtls -lapp_update -lbootloader_support -lspi_flash -lnvs_flash -lpthread -lesp_gdbstub -lespcoredump -lesp_phy -lesp_system -lesp_rom -lhal -lvfs -lesp_eth -ltcpip_adapter -lesp_netif -lesp_event -lwpa_supplicant -lesp_wifi -lconsole -llwip -llog -lheap -lsoc -lesp_hw_support -lxtensa -lesp_common -lesp_timer -lfreertos -lnewlib -lcxx -lapp_trace -lasio -lbt -lcbor -lunity -lcmock -lcoap -lnghttp -lesp-tls -lesp_adc_cal -lesp_hid -ltcp_transport -lesp_http_client -lesp_http_server -lesp_https_ota -lesp_lcd -lprotobuf-c -lprotocomm -lmdns -lesp_local_ctrl -lsdmmc -lesp_serial_slave_link -lesp_websocket_client -lexpat -lwear_levelling -lfatfs -lfreemodbus -ljsmn -ljson -llibsodium -lmqtt -lopenssl -lperfmon -lspiffs -lulp -lwifi_provisioning -lbutton -ljson_parser -ljson_generator -lesp_schedule -lesp_rainmaker -lqrcode -lws2812_led -lesp-dsp -lesp32-camera -lesp_littlefs -lfb_gfx -lasio -lcbor -lcmock -lunity -lcoap -lesp_lcd -lesp_local_ctrl -lesp_websocket_client -lexpat -lfreemodbus -ljsmn -llibsodium -lperfmon -lesp_adc_cal -lesp_hid -lfatfs -lwear_levelling -lopenssl -lspiffs -lesp_rainmaker -lmqtt -lwifi_provisioning -lprotocomm -lbt -lbtdm_app -lprotobuf-c -lmdns -ljson -ljson_parser -ljson_generator -lesp_schedule -lqrcode -lcat_face_detect -lhuman_face_detect -lcolor_detect -lmfn -ldl -lesp_ringbuf -lefuse -lesp_ipc -ldriver -lesp_pm -lmbedtls -lapp_update -lbootloader_support -lspi_flash -lnvs_flash -lpthread -lesp_gdbstub -lespcoredump -lesp_phy -lesp_system -lesp_rom -lhal -lvfs -lesp_eth -ltcpip_adapter -lesp_netif -lesp_event -lwpa_supplicant -lesp_wifi -lconsole -llwip -llog -lheap -lsoc -lesp_hw_support -lxtensa -lesp_common -lesp_timer -lfreertos -lnewlib -lcxx -lapp_trace -lnghttp -lesp-tls -ltcp_transport -lesp_http_client -lesp_http_server -lesp_https_ota -lsdmmc -lesp_serial_slave_link -lulp -lmbedtls -lmbedcrypto -lmbedx509 -lcoexist -lcore -lespnow -lmesh -lnet80211 -lpp -lsmartconfig -lwapi -lesp_ringbuf -lefuse -lesp_ipc -ldriver -lesp_pm -lmbedtls -lapp_update -lbootloader_support -lspi_flash -lnvs_flash -lpthread -lesp_gdbstub -lespcoredump -lesp_phy -lesp_system -lesp_rom -lhal -lvfs -lesp_eth -ltcpip_adapter -lesp_netif -lesp_event -lwpa_supplicant -lesp_wifi -lconsole -llwip -llog -lheap -lsoc -lesp_hw_support -lxtensa -lesp_common -lesp_timer -lfreertos -lnewlib -lcxx -lapp_trace -lnghttp -lesp-tls -ltcp_transport -lesp_http_client -lesp_http_server -lesp_https_ota -lsdmmc -lesp_serial_slave_link -lulp -lmbedtls -lmbedcrypto -lmbedx509 -lcoexist -lcore -lespnow -lmesh -lnet80211 -lpp -lsmartconfig -lwapi -lesp_ringbuf -lefuse -lesp_ipc -ldriver -lesp_pm -lmbedtls -lapp_update -lbootloader_support -lspi_flash -lnvs_flash -lpthread -lesp_gdbstub -lespcoredump -lesp_phy -lesp_system -lesp_rom -lhal -lvfs -lesp_eth -ltcpip_adapter -lesp_netif -lesp_event -lwpa_supplicant -lesp_wifi -lconsole -llwip -llog -lheap -lsoc -lesp_hw_support -lxtensa -lesp_common -lesp_timer -lfreertos -lnewlib -lcxx -lapp_trace -lnghttp -lesp-tls -ltcp_transport -lesp_http_client -lesp_http_server -lesp_https_ota -lsdmmc -lesp_serial_slave_link -lulp -lmbedtls -lmbedcrypto -lmbedx509 -lcoexist -lcore -lespnow -lmesh -lnet80211 -lpp -lsmartconfig -lwapi -lesp_ringbuf -lefuse -lesp_ipc -ldriver -lesp_pm -lmbedtls -lapp_update -lbootloader_support -lspi_flash -lnvs_flash -lpthread -lesp_gdbstub -lespcoredump -lesp_phy -lesp_system -lesp_rom -lhal -lvfs -lesp_eth -ltcpip_adapter -lesp_netif -lesp_event -lwpa_supplicant -lesp_wifi -lconsole -llwip -llog -lheap -lsoc -lesp_hw_support -lxtensa -lesp_common -lesp_timer -lfreertos -lnewlib -lcxx -lapp_trace -lnghttp -lesp-tls -ltcp_transport -lesp_http_client -lesp_http_server -lesp_https_ota -lsdmmc -lesp_serial_slave_link -lulp -lmbedtls -lmbedcrypto -lmbedx509 -lcoexist -lcore -lespnow -lmesh -lnet80211 -lpp -lsmartconfig -lwapi -lesp_ringbuf -lefuse -lesp_ipc -ldriver -lesp_pm -lmbedtls -lapp_update -lbootloader_support -lspi_flash -lnvs_flash -lpthread -lesp_gdbstub -lespcoredump -lesp_phy -lesp_system -lesp_rom -lhal -lvfs -lesp_eth -ltcpip_adapter -lesp_netif -lesp_event -lwpa_supplicant -lesp_wifi -lconsole -llwip -llog -lheap -lsoc -lesp_hw_support -lxtensa -lesp_common -lesp_timer -lfreertos -lnewlib -lcxx -lapp_trace -lnghttp -lesp-tls -ltcp_transport -lesp_http_client -lesp_http_server -lesp_https_ota -lsdmmc -lesp_serial_slave_link -lulp -lmbedtls -lmbedcrypto -lmbedx509 -lcoexist -lcore -lespnow -lmesh -lnet80211 -lpp -lsmartconfig -lwapi -lphy -lrtc -lesp_phy -lphy -lrtc -lesp_phy -lphy -lrtc -lxt_hal -lm -lnewlib -lstdc++ -lpthread -lgcc -lcxx -lapp_trace -lgcov -lapp_trace -lgcov -lc"

        env.Append(LIBS = arduino_cargocult.split())

        arduino_link_cargocult = "-T esp32.rom.redefined.ld -T memory.ld -T sections.ld -T esp32.rom.ld -T esp32.rom.api.ld -T esp32.rom.libgcc.ld -T esp32.rom.newlib-data.ld -T esp32.rom.syscalls.ld -T esp32.peripherals.ld"

        for linkscript in arduino_link_cargocult.split():
            if linkscript == "-T":
                continue
            env.Append(LINKFLAGS = ["-T", linkscript])
        env.Append(LINKFLAGS = ["-Wl,--undefined=uxTopUsedPriority"])

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

    def include_dirs(self, target):
        return self.common_include_dirs(target) + [
            # RULOS include dirs
            os.path.join(util.SRC_ROOT, "lib", "chip", "esp32"),

            # Espressif SDK include dirs
            os.path.join(self.sdk_root, "include", "config"),
            os.path.join(self.sdk_root, "include", "driver", "include"),
            os.path.join(self.sdk_root, "include", "esp32", "include"),
            os.path.join(self.sdk_root, "include", "esp_common", "include"),
            os.path.join(self.sdk_root, "include", "esp_eth", "include"),
            os.path.join(self.sdk_root, "include", "esp_event", "include"),
            os.path.join(self.sdk_root, "include", "esp_http_client", "include"),
            os.path.join(self.sdk_root, "include", "esp_hw_support", "include"),
            os.path.join(self.sdk_root, "include", "esp_hw_support", "include", "soc"),
            os.path.join(self.sdk_root, "include", "esp_ipc", "include"),
            os.path.join(self.sdk_root, "include", "esp_netif", "include"),
            os.path.join(self.sdk_root, "include", "esp_ringbuf", "include"),
            os.path.join(self.sdk_root, "include", "esp_rom", "include"),
            os.path.join(self.sdk_root, "include", "esp_system", "include"),
            os.path.join(self.sdk_root, "include", "esp_timer", "include"),
            os.path.join(self.sdk_root, "include", "esp_wifi", "include"),
            os.path.join(self.sdk_root, "include", "freertos", "include"),
            os.path.join(self.sdk_root, "include", "freertos", "include", "esp_additions"),
            os.path.join(self.sdk_root, "include", "freertos", "include", "esp_additions", "freertos"),
            os.path.join(self.sdk_root, "include", "freertos", "port", "xtensa", "include"),
            os.path.join(self.sdk_root, "include", "hal", "esp32", "include"),
            os.path.join(self.sdk_root, "include", "hal", "include"),
            os.path.join(self.sdk_root, "include", "hal", "platform_port", "include"),
            os.path.join(self.sdk_root, "include", "heap", "include"),
            os.path.join(self.sdk_root, "include", "lwip", "include", "apps"),
            os.path.join(self.sdk_root, "include", "lwip", "lwip", "src", "include"),
            os.path.join(self.sdk_root, "include", "lwip", "port", "esp32", "include"),
            os.path.join(self.sdk_root, "include", "newlib", "platform_include"),
            os.path.join(self.sdk_root, "include", "nghttp", "port", "include"),
            os.path.join(self.sdk_root, "include", "nvs_flash", "include"),
            os.path.join(self.sdk_root, "include", "soc", "esp32", "include"),
            os.path.join(self.sdk_root, "include", "soc", "include"),
            os.path.join(self.sdk_root, "include", "spi_flash", "include"),
            os.path.join(self.sdk_root, "include", "tcpip_adapter", "include"),
            os.path.join(self.sdk_root, "include", "xtensa", "esp32", "include"),
            os.path.join(self.sdk_root, "include", "xtensa", "include"),
        ]

    def ld_flags(self, target):
        return self.common_ld_flags(target) + [
            '-nostdlib',
        ]

    def cflags(self):
        return self.common_cflags() + [
            "-DRULOS_ESP32",
            "-DESP_PLATFORM",
            "-DESP32",
            "-DCORE_DEBUG_LEVEL=0",
            "-DGCC_NOT_5_2_0=0",
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
#            "-CC",
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
            f"{self.sdk_root}/bin/bootloader_dio_80m.bin",
            "0x10000",
            binfile.get_abspath(),
            "0x8000",
            partfile.get_abspath(),
        ]

    def find_esp32_port(self):
        if 'RULOS_ESP32_PORT' in os.environ:
            return os.environ['RULOS_ESP32_PORT']

        import serial.tools.list_ports as list_ports

        print("Port list:");
        for port in list_ports.comports():
            print(f"port {port}")
        #port_type = "CP2104"
        port_type = "CP2102"
        #port_type = "USB Single"   # TTGO
        esp32_ports = list(list_ports.grep(port_type))

        if len(esp32_ports) == 0:
            raise Exception(f"No {port_type} USB ports found; is your ESP32 plugged in?")

        if len(esp32_ports) > 1:
            raise Exception(f"Multiple {port_type} USB ports found. Unplug some, or specify with RULOS_ESP32_PORT env var")

        return esp32_ports[0].device

    def program_esp32(self, target, source, env):
        binfile = source[0]
        partfile = source[1]
        print("Programming ESP32...")

        usbpath = self.find_esp32_port()
        cmdline = self.programming_cmdline(usbpath, binfile, partfile)
        print(cmdline)
        subprocess.call(cmdline)

    def run_esp32(self, target, source, env):
        usbpath = self.find_esp32_port()
        subprocess.call(["grabserial", "-t", "-d", usbpath, "-b", "1000000", "-o", "esp32.log"])

    def run_esp32_epochsec(self, target, source, env):
        usbpath = self.find_esp32_port()
        subprocess.call(["grabserial", "-T", "-F", "%s.%f", "-d", usbpath, "-b", "1000000", "-o", "esp32.log"])

    def post_configure(self, env, outputs):
        binfile = env.MakeBin(outputs[0])
        partfile = env.MakePartMap(
            source=os.path.join(self.tool_root, "partitions", "default.csv"),
            target=f"{outputs[0][0].get_abspath()}.partitions.bin",
        )
        Default([binfile, partfile])

        # Create programming aliases so a command like "scons
        # program-esp32-<progname>" works, even for SConstruct files
        # that have multiple targets
        prog_target = f"esp32-{os.path.basename(env['RulosTargetName'])}"
        program = env.Alias(
            f"program-{prog_target}",
            [binfile, partfile] + outputs,
            self.program_esp32)
        env.Alias(
            f"run-{prog_target}",
            program,
            self.run_esp32)
        env.Alias(
            f"rune-{prog_target}",
            program,
            self.run_esp32_epochsec)

        # Simpler "scons program" and "scons run" commands for simple
        # SConstruct files that have only a single target
        env.Alias("program", f"program-{prog_target}")
        env.Alias("run", f"run-{prog_target}")
        env.Alias("rune", f"rune-{prog_target}")
