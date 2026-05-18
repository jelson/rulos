#!/usr/bin/env python3
#
# Flash / reset an STM32 (or any SWD target) over an attached Black
# Magic Probe. App-agnostic: importable library plus a thin CLI.
#
#   import bmpflash
#   bmpflash.flash_elf("build/foo/arm-stm32h523xc/foo.elf")  # flash+verify
#   bmpflash.reset()                                          # cold reset
#
#   bmpflash.py firmware.elf            # flash + verify + reset into it
#   bmpflash.py firmware.elf --erase    # mass-erase first (unbrick)
#   bmpflash.py --reset-only            # just reset the target
#   bmpflash.py firmware.elf --port /dev/ttyACMx   # skip BMP autodetect
#
# The BMP GDB port is found by USB enumeration (its stable
# /dev/serial/by-id name), not a fixed ttyACMx -- ttyACM numbering
# depends on plug order and whatever else is connected.

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


def _gdb(port, elf=None, load=False, mass_erase=False):
    """Drive the BMP: attach, optionally mass-erase, optionally
    load+verify, reset, detach. `elf` supplies symbols for `load`; a
    reset-only run needs no file. Returns gdb's exit code.

    The monitor command spelling is BMP-firmware-version sensitive;
    this exact vector is the known-good one -- do not "simplify" it."""
    print(f"bmpflash: BMP GDB port: {port}", file=sys.stderr)
    cmd = ["gdb-multiarch"]
    if elf is not None:
        cmd.append(elf)
    cmd += ["-ex", "set confirm off",
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


def flash_elf(elf, port=None, mass_erase=False):
    """Flash `elf` over the BMP, verify it, and reset into it.
    Optionally mass-erase first (clears reserved/nvconfig sectors --
    the only way to recover a unit bricked by torn flash). Returns
    gdb's exit code (0 = success)."""
    return _gdb(port or detect_bmp(), elf=elf, load=True,
                mass_erase=mass_erase)


def reset(port=None):
    """Cold-reset the target into its existing image (no flash): clears
    SRAM and re-runs boot, so flash-persisted state can be verified to
    survive a power-loss-equivalent restart. Returns gdb's exit code."""
    return _gdb(port or detect_bmp(), load=False)


def main():
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("elf", nargs="?", default=None,
                   help="ELF to flash (omit with --reset-only)")
    p.add_argument("--reset-only", action="store_true",
                   help="just reset the target (no flash); proves "
                        "flash-persisted state survives a cold restart")
    p.add_argument("--erase", action="store_true",
                   help="mass-erase the whole chip before flashing "
                        "(clears reserved/nvconfig sectors; recovers a "
                        "unit bricked by torn flash)")
    p.add_argument("--port", help="override BMP autodetection")
    args = p.parse_args()
    try:
        if args.reset_only:
            rc = reset(port=args.port)
        elif args.elf:
            rc = flash_elf(args.elf, port=args.port, mass_erase=args.erase)
        else:
            p.error("give an ELF to flash, or --reset-only")
    except RuntimeError as e:
        sys.exit(f"bmpflash: {e}")
    sys.exit(rc)


if __name__ == "__main__":
    main()
