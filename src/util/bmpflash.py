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


def bmp_present(port=None):
    """True iff a Black Magic Probe is attached. Pure USB enumeration
    lookup -- touches no SWD target, so it is safe to call as a
    precondition without perturbing anything."""
    try:
        detect_bmp() if port is None else None
        return True
    except RuntimeError:
        return False


def read_serial(uid_base, prefix="", port=None):
    """Read the SWD target's 96-bit unique ID over the probe and render
    it the way the firmware renders its USB iSerialNumber, so the two
    can be compared: `prefix` then the three 32-bit UID words at
    uid_base, uid_base+4, uid_base+8, each as 8 uppercase hex nibbles
    MSB-first (matching usbd_cdc_get_serial / uid_hex in usbd_desc.c).
    Returns the string, or None if no target responds.

    Reads memory only -- no flash, no reset. nRST-on-connect is
    disabled so a running target is not pulsed, and the target is
    `detach`ed (resumed), not killed, so it keeps running afterwards.
    uid_base is MCU-family specific (STM32H5: 0x08FFF800). Raises
    RuntimeError if no probe is attached."""
    port = port or detect_bmp()
    out = subprocess.run(
        ["gdb-multiarch",
         "-ex", "set confirm off",
         "-ex", "set pagination off",
         # The UID lives outside the probe's declared flash/RAM map;
         # without this gdb refuses the read as "Cannot access memory".
         "-ex", "set mem inaccessible-by-default off",
         "-ex", f"tar ext {port}",
         "-ex", "mon connect_rst disable",
         "-ex", "mon swd",
         "-ex", "at 1",
         "-ex", f"x/3xw {uid_base:#x}",
         "-ex", "detach",
         "-ex", "quit"],
        capture_output=True, text=True)
    text = out.stdout + out.stderr
    # The x/3xw dump prints the three words as 0x-prefixed tokens on the
    # line(s) following the address label; collect them in order.
    words = []
    for line in text.splitlines():
        if ":" in line and "0x" in line.split(":", 1)[1]:
            for tok in line.split(":", 1)[1].split():
                if tok.startswith("0x") and len(tok) == 10:
                    words.append(int(tok, 16))
    if len(words) < 3:
        return None
    return prefix + "".join(f"{w:08X}" for w in words[:3])


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
