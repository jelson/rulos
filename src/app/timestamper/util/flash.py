#!/usr/bin/env python3
#
# Build/flash/reset the LectroTIC-4 over its attached Black Magic
# Probe. Importable library (flash_firmware / reset_dut, used by
# regression_test.py) plus a thin CLI.
#
# The BMP GDB port is found by USB enumeration (its stable
# /dev/serial/by-id name), not a fixed ttyACMx -- ttyACM numbering
# depends on plug order and what else (incl. the LectroTIC-4 itself)
# is connected.

import argparse
import glob
import os
import subprocess
import sys


def detect_bmp():
    """Path of the BMP GDB server. The probe exposes two CDC-ACM
    interfaces: -if00 is the GDB server (driven here), -if02 its UART
    passthrough. Raises RuntimeError if not exactly one is present."""
    matches = sorted(glob.glob(
        "/dev/serial/by-id/usb-Black_Magic*-if00"))
    if not matches:
        raise RuntimeError(
            "no Black Magic Probe found (looked for "
            "/dev/serial/by-id/usb-Black_Magic*-if00). Plugged in? "
            "Pass port= to override.")
    if len(matches) > 1:
        raise RuntimeError("multiple Black Magic Probes:\n  " +
                           "\n  ".join(matches) + "\nPass port= to pick one.")
    return os.path.realpath(matches[0])


def repo_root():
    return subprocess.check_output(
        ["git", "-C", os.path.dirname(os.path.abspath(__file__)),
         "rev-parse", "--show-toplevel"], text=True).strip()


def default_elf():
    return os.path.join(
        repo_root(), "build/timestamper/arm-stm32h523xc/timestamper.elf")


def build():
    """Build the firmware (scons in the timestamper app dir)."""
    subprocess.run(["scons"], check=True,
                    cwd=os.path.join(repo_root(), "src/app/timestamper"))


def _gdb(elf, port, load, mass_erase=False):
    """Drive the BMP: attach, optionally mass-erase, optionally
    load+verify, reset, detach. Returns gdb's exit code."""
    print(f"flash.py: BMP GDB port: {port}", file=sys.stderr)
    cmd = ["gdb-multiarch", elf,
           "-ex", "set confirm off",
           "-ex", "set pagination off",
           "-ex", f"tar ext {port}",
           "-ex", "mon conn enable",
           "-ex", "mon swd",
           "-ex", "at 1",
           *(["-ex", "mon erase_mass"] if mass_erase else []),
           *(["-ex", "load", "-ex", "compare-sections"] if load else []),
           "-ex", "mon reset",
           "-ex", "kill",
           "-ex", "quit"]
    return subprocess.run(cmd).returncode


def flash_firmware(elf=None, port=None, do_build=True, mass_erase=False):
    """Build (unless do_build=False), optionally mass-erase the whole
    chip first (clears any reserved/nvconfig sectors -- the only way to
    recover a unit bricked by torn flash), flash, verify, and reset
    into the new image. Returns gdb's exit code."""
    if do_build:
        build()
    return _gdb(elf or default_elf(), port or detect_bmp(), load=True,
                mass_erase=mass_erase)


def reset_dut(port=None):
    """Cold-reset the DUT into its existing image (no flash): clears
    SRAM and re-runs boot, so flash-persisted state can be verified to
    survive a power-loss-equivalent restart. Returns gdb's exit code."""
    return _gdb(default_elf(), port or detect_bmp(), load=False)


def main():
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("elf", nargs="?", default=None,
                   help="ELF to flash (default: the timestamper build)")
    p.add_argument("--no-build", action="store_true",
                   help="flash the existing elf as-is")
    p.add_argument("--reset-only", action="store_true",
                   help="just reset the DUT (no build/flash); proves "
                        "flash-persisted config survives a cold restart")
    p.add_argument("--erase", action="store_true",
                   help="mass-erase the whole chip before flashing "
                        "(clears reserved/nvconfig sectors; recovers a "
                        "unit bricked by torn flash)")
    p.add_argument("--port", help="override BMP autodetection")
    args = p.parse_args()
    try:
        if args.reset_only:
            rc = reset_dut(port=args.port)
        else:
            rc = flash_firmware(elf=args.elf, port=args.port,
                                do_build=not args.no_build,
                                mass_erase=args.erase)
    except RuntimeError as e:
        sys.exit(f"flash.py: {e}")
    sys.exit(rc)


if __name__ == "__main__":
    main()
