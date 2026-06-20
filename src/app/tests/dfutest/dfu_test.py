#!/usr/bin/env python3
#
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

"""End-to-end test of the shared RULOS DFU facility on the STM32H523.

  1. Build dfutest as "Rev A".
  2. Flash Rev A with the Black Magic Probe (the trusted, out-of-band
     baseline path).
  3. Open the USB CDC port; assert the banner says FW=Rev A.
  4. Build dfutest as "Rev B".
  5. One dfu-util command -- the exact one we tell users to run --
     with the two-tuple -d <runtime>,<dfu-mode>: it auto-sends
     DFU_DETACH (the facility under test reboots into the H5 ROM
     bootloader), waits for the re-enumeration, writes Rev B, and
     ":leave" runs it.
  6. Re-open the CDC port; assert the banner now says FW=Rev B.

Only step 2 uses the SWD probe; the A->B update in step 5 is pure USB
in a single dfu-util invocation, exactly what an end user runs. SWD is
also the recovery path if anything goes wrong, so a failed run never
bricks the board -- re-run, or flash any known-good image over SWD.

Run from this directory:  ./dfu_test.py
"""

import glob
import os
import re
import subprocess
import sys
import tempfile
import time

import serial

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
sys.path.insert(0, os.path.join(REPO_ROOT, "src", "util"))
import bmpflash  # noqa: E402  (after sys.path setup)

# Must match SConstruct's USBD_VID/PID and the H5 system-bootloader DFU.
RUNTIME_USB_ID = "1209:df00"
ROM_DFU_USB_ID = "0483:df11"
FLASH_ORIGIN = 0x08000000
BANNER_RE = re.compile(rb"DFUTEST FW=(\S+)")


def run(cmd, **kw):
    print(f"  $ {' '.join(cmd)}")
    return subprocess.run(cmd, check=True, **kw)


def build(rev):
    """Build dfutest with its revision string baked in via the build
    system's --define (no source-file mutation) into a per-rev build
    tree via --build-dir, so RevA and RevB coexist. Returns the
    (elf, bin) paths; the build system emits the .bin automatically."""
    build_dir = os.path.join(tempfile.gettempdir(), f"dfutest-build-{rev}")
    run(["scons", f"--build-dir={build_dir}", f'--define=DFUTEST_FW_VERSION=\\"{rev}\\"'], cwd=HERE)
    stem = os.path.join(build_dir, "dfutest", "arm-stm32h523xc", "dfutest")
    return stem + ".elf", stem + ".bin"


def find_cdc():
    """The dfutest CDC port, by its distinct USB serial prefix."""
    hits = sorted(glob.glob("/dev/serial/by-id/*DFUT-*-if*"))
    if not hits:
        hits = sorted(glob.glob("/dev/serial/by-id/*RULOS-DFUTEST*"))
    if not hits:
        sys.exit("ERROR: dfutest CDC port not found in /dev/serial/by-id")
    return hits[0]


def read_banner(timeout_s=15):
    """Open the CDC port and return the FW=<rev> string from its banner."""
    deadline = time.time() + timeout_s
    port = None
    while time.time() < deadline:
        try:
            port = find_cdc()
            break
        except SystemExit:
            time.sleep(0.5)
    if port is None:
        sys.exit("ERROR: dfutest CDC port never appeared")
    # The device greets on connect (pyserial asserts DTR on open) and
    # then re-emits the banner once a second.
    with serial.Serial(port, 115200, timeout=1) as s:
        end = time.time() + timeout_s
        buf = b""
        while time.time() < end:
            buf += s.read(256)
            m = BANNER_RE.search(buf)
            if m:
                return m.group(1).decode()
    sys.exit("ERROR: no DFUTEST banner seen on the CDC port")


def dfu_update(binpath):
    """The single-shot user-facing update command. dfu-util's two-tuple
    -d <runtime>,<dfu-mode> matches the running app, auto-sends
    DFU_DETACH, waits for the device to re-enumerate as the ST ROM
    bootloader, downloads, and ':leave' runs the new image -- all in
    one invocation. This is exactly what we tell users to run.

    The ST ROM bootloader resets into the freshly written app the
    instant it gets the leave request, so it never answers dfu-util's
    final get_status -- dfu-util then prints 'Error during download
    get_status' and exits 74 even though flash + leave both succeeded.
    'File downloaded successfully' is the real success signal (the
    [5/5] banner read is the definitive proof)."""
    # dfu-util needs raw USB access -> sudo.
    cmd = [
        "sudo",
        "dfu-util",
        "-d",
        f"{RUNTIME_USB_ID},{ROM_DFU_USB_ID}",
        "-a",
        "0",
        "-s",
        f"0x{FLASH_ORIGIN:08x}:leave",
        "-D",
        binpath,
    ]
    print(f"  $ {' '.join(cmd)}")
    p = subprocess.run(cmd, capture_output=True, text=True)
    sys.stdout.write(p.stdout)
    if "File downloaded successfully" not in p.stdout:
        sys.stdout.write(p.stderr)
        sys.exit(f"FAIL: dfu-util update did not complete " f"(exit {p.returncode})")
    if p.returncode != 0:
        print(
            f"  (dfu-util exit {p.returncode} after a successful "
            f"download is the benign get_status-after-leave quirk; "
            f"the banner check below is authoritative)"
        )


def main():
    print("=== RULOS DFU facility end-to-end test (STM32H523) ===")

    # ---- Rev A via the Black Magic Probe -----------------------------
    print("\n[1/6] Build Rev A")
    elf_a, _ = build("RevA")

    print("\n[2/6] Flash Rev A over SWD (Black Magic Probe)")
    if bmpflash.flash_elf(elf_a) != 0:
        sys.exit("FAIL: BMP flash of Rev A failed (see gdb output above)")

    print("\n[3/6] Verify the device reports Rev A")
    rev = read_banner()
    print(f"  device reports FW={rev}")
    if rev != "RevA":
        sys.exit(f"FAIL: expected RevA after BMP flash, got {rev}")
    print("  OK")

    # ---- Rev B purely over USB via dfu-util --------------------------
    print("\n[4/6] Build Rev B")
    _, bin_b = build("RevB")

    print("\n[5/6] Single-shot dfu-util update (detach + download + leave)")
    dfu_update(bin_b)

    print("\n[6/6] Verify the device now reports Rev B")
    rev = read_banner()
    print(f"  device reports FW={rev}")
    if rev != "RevB":
        sys.exit(f"FAIL: expected RevB after dfu-util update, got {rev}")

    print("\n=== PASS: A->B firmware update over USB DFU confirmed ===")


if __name__ == "__main__":
    main()
