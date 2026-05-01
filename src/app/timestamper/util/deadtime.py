#!/usr/bin/env python3

"""Binary-search for the timestamper's dead time.

Drives a Rigol DG1022Z (via siggen.py's Siggen class) to produce pulse
bursts at various spacings while reading the timestamper's serial output
to determine whether all pulses in each burst are captured.

Usage:
  deadtime.py                           defaults: 3 pulses, 100-1000 ns
  deadtime.py --ncyc 1000              test with 1000-pulse bursts
  deadtime.py --bottom 50 --top 500     custom range in ns
  deadtime.py --port /dev/ttyACM1       custom serial port
  deadtime.py --host siggen2            custom signal generator hostname
"""

import argparse
import sys
from dataclasses import dataclass

from tsctl import Timestamper
from siggen import Siggen


@dataclass
class PulseCount:
    timestamps: int    # number of timestamp lines received
    overcaptures: int  # number of "overcapture" comment lines
    overflows: int     # number of "buf overflow" comment lines
    burst_sizes: list  # list of per-burst timestamp counts


def count_pulses(ts, settle_time_s=2.0, measure_time_s=3.0,
                 binary=False, verbose=True):
    """Read records via the Timestamper, classify, and group into bursts."""
    for rec in ts.read_for(settle_time_s, binary):
        if verbose and rec[0] == "comment":
            print(f"    | {rec[1]}")

    timestamps = []  # list of float seconds-since-boot
    overcaptures = 0
    overflows = 0
    for rec in ts.read_for(measure_time_s, binary):
        if rec[0] == "comment":
            if verbose:
                print(f"    | {rec[1]}")
            text = rec[1].lower()
            if "overcapture" in text:
                overcaptures += 1
            if "overflow" in text:
                overflows += 1
            continue
        _, ch, sec, ns = rec
        timestamps.append(sec + ns * 1e-9)
        if verbose:
            print(f"    | {ch} {sec}.{ns:09d}")

    # Group timestamps into bursts. Bursts repeat 1/sec, so timestamps
    # within 0.5s of each other belong to the same burst.
    burst_sizes = []
    i = 0
    while i < len(timestamps):
        burst_size = 1
        t_start = timestamps[i]
        j = i + 1
        while j < len(timestamps) and abs(timestamps[j] - t_start) < 0.5:
            burst_size += 1
            j += 1
        burst_sizes.append(burst_size)
        i = j

    return PulseCount(
        timestamps=len(timestamps),
        overcaptures=overcaptures,
        overflows=overflows,
        burst_sizes=burst_sizes,
    )


def test_spacing(sg, ts, ns, ncyc=3, binary=False, verbose=True):
    """Set the signal generator to the given spacing and check results.

    Returns True if all pulses in each burst are reliably captured
    (no dead time issue), False if pulses are being missed.
    """
    print(f"  Testing {ns:.1f} ns ({ncyc} pulses/burst)...")

    sg.pulse_burst(ns, ncyc=ncyc)

    result = count_pulses(ts, binary=binary, verbose=verbose)

    # Pass = every burst has all ncyc pulses, no overcaptures or overflows.
    # Ignore partial bursts at measurement window boundaries.
    complete = [sz for sz in result.burst_sizes if sz >= ncyc]

    if len(complete) < 2:
        passed = False
    else:
        passed = (result.overcaptures == 0 and result.overflows == 0)

    status = "PASS" if passed else "FAIL"
    burst_str = ",".join(str(s) for s in result.burst_sizes)
    print(f"  {ns:6.1f} ns: {status}  "
          f"(timestamps={result.timestamps}, "
          f"bursts=[{burst_str}], "
          f"overcaptures={result.overcaptures}, "
          f"overflows={result.overflows})")

    return passed


def main():
    parser = argparse.ArgumentParser(
        description="Binary-search for timestamper dead time")
    parser.add_argument("--bottom", type=float, default=100,
                        help="Lower bound in ns (default: 100)")
    parser.add_argument("--top", type=float, default=1000,
                        help="Upper bound in ns (default: 1000)")
    parser.add_argument("--port", default=None,
                        help="Timestamper serial port (default: autodetect)")
    parser.add_argument("--host", default="siggen",
                        help="Signal generator hostname (default: siggen)")
    parser.add_argument("--precision", type=float, default=5,
                        help="Stop when range narrows to this many ns "
                             "(default: 5)")
    parser.add_argument("--ncyc", type=int, default=3,
                        help="Pulses per burst (default: 3)")
    parser.add_argument("--text", action="store_true",
                        help="Use TEXT wire format instead of BINARY "
                             "(default)")
    parser.add_argument("--quiet", "-q", action="store_true",
                        help="Suppress raw serial output")
    args = parser.parse_args()

    lo = args.bottom
    hi = args.top
    ncyc = args.ncyc
    verbose = not args.quiet
    binary = not args.text

    with Timestamper(args.port) as ts:
        print(f"Searching for dead time in range [{lo:.0f}, {hi:.0f}] ns, "
              f"{ncyc} pulses/burst")
        print(f"Timestamper port: {ts.port}")
        print(f"Signal generator: {args.host}")
        print(f"Wire format:      {'BINARY' if binary else 'TEXT'}")
        print()

        ts.set_format(binary)
        ts.set_stream_enabled(True)

        try:
            with Siggen(host=args.host) as sg:
                print(f"Connected to: {sg.idn()}")
                print()

                # Verify endpoints: top should pass, bottom should fail.
                # If --bottom == --top, the upper-bound test answers both.
                print("Verifying upper bound...")
                if not test_spacing(sg, ts, hi, ncyc=ncyc, binary=binary,
                                    verbose=verbose):
                    print(f"ERROR: Upper bound {hi:.0f} ns fails -- "
                          f"increase --top")
                    sys.exit(1)

                if lo == hi:
                    print(f"NOTE: --bottom == --top == {lo:.0f} ns; that "
                          f"spacing passes -- dead time is at or below "
                          f"{lo:.0f} ns")
                    sg.output_off()
                    sys.exit(0)

                print("Verifying lower bound...")
                if test_spacing(sg, ts, lo, ncyc=ncyc, binary=binary,
                                verbose=verbose):
                    print(f"NOTE: Lower bound {lo:.0f} ns already passes "
                          f"-- decrease --bottom or dead time is below "
                          f"{lo:.0f} ns")
                    sg.output_off()
                    sys.exit(0)

                print()
                print("Binary search:")

                while (hi - lo) > args.precision:
                    mid = (lo + hi) / 2
                    mid = round(mid)
                    if mid == lo or mid == hi:
                        break
                    if test_spacing(sg, ts, mid, ncyc=ncyc, binary=binary,
                                    verbose=verbose):
                        hi = mid
                    else:
                        lo = mid

                print()
                print(f"Dead time is between {lo:.0f} ns and {hi:.0f} ns")
                sg.output_off()
                print("Signal generator output off.")
        finally:
            ts.set_format(False)


if __name__ == "__main__":
    main()
