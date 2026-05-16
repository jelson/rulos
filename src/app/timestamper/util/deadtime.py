#!/usr/bin/env python3

"""Binary-search for the LectroTIC-4's dead time.

Drives a Rigol DG1022Z (via siggen.py's Siggen class) to produce pulse
bursts at various spacings while reading the LectroTIC-4's serial output
to determine whether all pulses in each burst are captured.

Usage:
  deadtime.py                           defaults: 16383-pulse bursts,
                                                  100-1000 ns range
  deadtime.py --ncyc 4096              just the per-channel DMA buffer
  deadtime.py --bottom 50 --top 500     custom range in ns
  deadtime.py --port /dev/ttyACM1       custom serial port
  deadtime.py --host siggen2            custom signal generator hostname
  deadtime.py --channel 2               sub-channel to score (default 0)

The pass/fail signal is the device's TEXT-mode '#' overcapture /
overflow lines, so this always reads the TEXT stream (parsed with
regression_test.parse_text_stream -- the shared text reader).
"""

import argparse
import sys
from dataclasses import dataclass

from tsctl import LectroTIC4
from siggen import Siggen
from regression_test import parse_text_stream


@dataclass
class PulseCount:
    timestamps: int    # number of timestamp lines received
    overcaptures: int  # number of "overcapture" comment lines
    overflows: int     # number of "buf overflow" comment lines
    burst_sizes: list  # list of per-burst timestamp counts


def count_pulses(tic, channel, measure_time_s=3.0, verbose=True):
    """Capture the device's TEXT stream for measure_time_s and group the
    `channel` sub-channel's timestamps into bursts. Overcapture/overflow
    counts come from the '#' diagnostic lines (summed across channels)."""
    tic.send("FORM:DATA TEXT")
    tic.set_stream_enabled(True)
    # Drop the device's ring + host buffer so we measure only bursts
    # captured under the current siggen configuration.
    tic.discard_pending()

    times, others, overcaptures, overflows, comments = \
        parse_text_stream(tic.read_raw(measure_time_s), channel)
    if verbose:
        for c in comments:
            print(f"    | {c}")
        for t in times:
            print(f"    | ch{channel} {t:.9f}")
    timestamps = times

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


def test_spacing(sg, tic, ns, channel, ncyc=3, verbose=True):
    """Set the signal generator to the given spacing and check results.

    Returns True if all pulses in each burst are reliably captured
    (no dead time issue), False if pulses are being missed.
    """
    print(f"  Testing {ns:.1f} ns ({ncyc} pulses/burst)...")

    sg.pulse_burst(ns, ncyc=ncyc)

    result = count_pulses(tic, channel, verbose=verbose)

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
        description="Binary-search for LectroTIC-4 dead time")
    parser.add_argument("--bottom", type=float, default=100,
                        help="Lower bound in ns (default: 100)")
    parser.add_argument("--top", type=float, default=1000,
                        help="Upper bound in ns (default: 1000)")
    parser.add_argument("--port", default=None,
                        help="LectroTIC-4 serial port (default: autodetect)")
    parser.add_argument("--host", default="siggen",
                        help="Signal generator hostname (default: siggen)")
    parser.add_argument("--precision", type=float, default=5,
                        help="Stop when range narrows to this many ns "
                             "(default: 5)")
    parser.add_argument("--ncyc", type=int, default=16383,
                        help="Pulses per burst. The internal timestamp "
                             "ring holds 16383 (16k-1, classic ring-buffer "
                             "off-by-one); 4096 exercises just the "
                             "per-channel DMA buffer. Smaller values fit "
                             "in hardware and give no useful dead-time "
                             "data. Default: 16383.")
    parser.add_argument("--channel", type=int, default=0,
                        help="Sub-channel whose burst completeness is "
                             "scored. The signal generator drives one "
                             "input; that input's rising sub-channel is "
                             "ch0, falling ch1, etc. (default: 0)")
    parser.add_argument("--quiet", "-q", action="store_true",
                        help="Suppress raw serial output")
    args = parser.parse_args()

    lo = args.bottom
    hi = args.top
    ncyc = args.ncyc
    verbose = not args.quiet
    channel = args.channel

    with LectroTIC4(args.port) as tic:
        print(f"Searching for dead time in range [{lo:.0f}, {hi:.0f}] ns, "
              f"{ncyc} pulses/burst")
        print(f"LectroTIC-4 port: {tic.port}")
        print(f"Signal generator: {args.host}")
        print(f"Scoring sub-channel: {channel}")
        print()

        with Siggen(host=args.host) as sg:
            print(f"Connected to: {sg.idn()}")
            print()

            # Verify endpoints: top should pass, bottom should fail.
            # If --bottom == --top, the upper-bound test answers both.
            print("Verifying upper bound...")
            if not test_spacing(sg, tic, hi, channel, ncyc=ncyc,
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
            if test_spacing(sg, tic, lo, channel, ncyc=ncyc,
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
                if test_spacing(sg, tic, mid, channel, ncyc=ncyc,
                                verbose=verbose):
                    hi = mid
                else:
                    lo = mid

            print()
            print(f"Dead time is between {lo:.0f} ns and {hi:.0f} ns")
            sg.output_off()
            print("Signal generator output off.")


if __name__ == "__main__":
    main()
