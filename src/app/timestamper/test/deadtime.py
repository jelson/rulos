#!/usr/bin/env python3

"""Binary-search for the LectroTIC-4's dead time.

Drives a Lectrobox PG-4 (pgctl) over USB to produce N-cycle pulse
bursts at various spacings while reading the LectroTIC-4's serial
output to determine whether all pulses in each burst are captured.

Usage:
  deadtime.py                           defaults: 16383-pulse bursts,
                                                  100-1000 ns range
  deadtime.py --ncyc 4096              just the per-channel DMA buffer
  deadtime.py --bottom 50 --top 500     custom range in ns
  deadtime.py --port /dev/ttyACM1       custom serial port
  deadtime.py --pg-port /dev/ttyACM2    custom PG-4 serial port
  deadtime.py --channel 2               channel under test (default 0)

The pass/fail signal is the device's loss reporting. In BINARY mode
(default) it counts the in-band PULSES_LOST records the firmware emits,
decoded by the tsctl library; with --text it reads the ASCII stream
and counts the "# ch<N>: ..." overcapture/overflow lines instead (via
regression_test.parse_text_stream, the shared text reader). The two
modes have different USB-drain ceilings, so dead time can differ
between them once a burst exceeds the on-device ring.
"""

import argparse
import os
import sys
from dataclasses import dataclass

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "util"))
sys.path.insert(0, os.path.join(
    os.path.dirname(__file__), "..", "..", "pulsegen", "util"))

from tsctl import LectroTIC4, Timestamp, PulsesLost, OscillatorFailure
from pgctl import Pulsegen
from regression_test import parse_text_stream


@dataclass
class PulseCount:
    timestamps: int    # number of timestamp lines received
    overcaptures: int  # number of "overcapture" comment lines
    overflows: int     # number of "buf overflow" comment lines
    bursts: list       # list of per-burst timestamp lists (seconds)

    @property
    def burst_sizes(self):
        return [len(b) for b in self.bursts]


def count_pulses(tic, channel, measure_time_s=3.0, text=False, verbose=True):
    """Capture the device's stream for measure_time_s and group the
    `channel` sub-channel's timestamps into bursts. In BINARY mode
    (default) overcapture / overflow counts come from the in-band
    PULSES_LOST records (summed across channels); with text=True they
    come from the ASCII "# ch<N>: ..." diagnostic lines, both via
    regression_test.parse_text_stream."""
    if text:
        # Drop the device's ring + host buffer so we measure only bursts
        # captured under the current PG-4 configuration.
        tic.send("FORM:DATA TEXT")
        tic.set_stream_enabled(True)
        tic.discard_pending()
        timestamps, _pols, _others, overcaptures, overflows, _comments = \
            parse_text_stream(tic.read_raw(measure_time_s), channel)
    else:
        tic.discard_pending()
        timestamps = []
        overcaptures = overflows = 0
        for r in tic.read_for(measure_time_s):
            if isinstance(r, Timestamp):
                if r.channel == channel:
                    timestamps.append(r.seconds + r.nanoseconds * 1e-9)
            elif isinstance(r, PulsesLost):
                overcaptures += r.overcaptures
                overflows += r.buf_overflows
            elif isinstance(r, OscillatorFailure):
                raise RuntimeError("reference clock failed -- device halted; "
                                   "restore the 10 MHz source and reset")
    if verbose:
        for t in timestamps:
            print(f"    | ch{channel} {t:.9f}")

    # Group timestamps into bursts. Bursts repeat 1/sec, so timestamps
    # within 0.5s of each other belong to the same burst.
    bursts = []
    i = 0
    while i < len(timestamps):
        t_start = timestamps[i]
        j = i + 1
        while j < len(timestamps) and abs(timestamps[j] - t_start) < 0.5:
            j += 1
        bursts.append(timestamps[i:j])
        i = j

    return PulseCount(
        timestamps=len(timestamps),
        overcaptures=overcaptures,
        overflows=overflows,
        bursts=bursts,
    )


def test_spacing(sg, tic, ns, channel, ncyc=3, text=False, verbose=True):
    """Set the signal generator to the given spacing and check results.

    Returns True if all pulses in each burst are reliably captured
    (no dead time issue), False if pulses are being missed.
    """
    print(f"  Testing {ns:.1f} ns ({ncyc} pulses/burst)...")

    sg.pulse_burst(channel, ns, ncyc)

    result = count_pulses(tic, channel, text=text, verbose=verbose)

    # Pass = every burst has all ncyc pulses, no overcaptures or
    # overflows, and every intra-burst gap matches the requested
    # spacing -- a count can be exact while the values are wrong (a
    # mis-paired seconds word, a mis-programmed generator). Ignore
    # partial bursts at measurement window boundaries.
    complete = [b for b in result.bursts if len(b) >= ncyc]

    expected_s = max(ns, sg.MIN_BURST_SPACING_NS) * 1e-9
    gap_tol_s = 12e-9
    bad_gaps = 0
    worst_dev_ns = 0.0
    for b in complete:
        for i in range(1, len(b)):
            dev = (b[i] - b[i - 1]) - expected_s
            if abs(dev) > abs(worst_dev_ns) * 1e-9:
                worst_dev_ns = dev * 1e9
            if abs(dev) > gap_tol_s:
                bad_gaps += 1

    if len(complete) < 2:
        passed = False
    else:
        passed = (result.overcaptures == 0 and result.overflows == 0
                  and bad_gaps == 0)

    status = "PASS" if passed else "FAIL"
    burst_str = ",".join(str(s) for s in result.burst_sizes)
    print(f"  {ns:6.1f} ns: {status}  "
          f"(timestamps={result.timestamps}, "
          f"bursts=[{burst_str}], "
          f"overcaptures={result.overcaptures}, "
          f"overflows={result.overflows}, "
          f"bad_gaps={bad_gaps}, "
          f"worst_gap_dev={worst_dev_ns:+.1f} ns)")

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
    parser.add_argument("--pg-port", default=None,
                        help="PG-4 serial port (default: autodetect)")
    parser.add_argument("--precision", type=float, default=5,
                        help="Stop when range narrows to this many ns "
                             "(default: 5)")
    parser.add_argument("--ncyc", type=int, default=16383,
                        help="Pulses per burst. The internal timestamp "
                             "ring holds 16383 (16k-1, classic ring-buffer "
                             "off-by-one); 2048 exercises just the "
                             "per-sub-stream DMA buffer. Smaller values "
                             "fit in hardware and give no useful "
                             "dead-time data. Default: 16383.")
    parser.add_argument("--channel", type=int, default=0,
                        choices=[0, 1, 2, 3],
                        help="Channel under test (0-3). The PG-4 output "
                             "wired straight through to this jack is "
                             "driven and its burst completeness scored "
                             "(default: 0)")
    parser.add_argument("--text", action="store_true",
                        help="Score the ASCII TEXT stream instead of the "
                             "BINARY union (different USB-drain ceiling)")
    parser.add_argument("--quiet", "-q", action="store_true",
                        help="Suppress raw serial output")
    args = parser.parse_args()

    lo = args.bottom
    hi = args.top
    ncyc = args.ncyc
    verbose = not args.quiet
    channel = args.channel
    text = args.text

    with LectroTIC4(args.port) as tic:
        print(f"Searching for dead time in range [{lo:.0f}, {hi:.0f}] ns, "
              f"{ncyc} pulses/burst")
        print(f"LectroTIC-4 port: {tic.port}")
        print(f"Channel under test: {channel}")
        print(f"Wire format: {'TEXT' if text else 'BINARY'}")
        print()

        with Pulsegen(port=args.pg_port) as sg:
            print(f"Connected to: {sg.idn()}")
            print()

            # Verify endpoints: top should pass, bottom should fail.
            # If --bottom == --top, the upper-bound test answers both.
            print("Verifying upper bound...")
            if not test_spacing(sg, tic, hi, channel, ncyc=ncyc,
                                text=text, verbose=verbose):
                print(f"ERROR: Upper bound {hi:.0f} ns fails -- "
                      f"increase --top")
                sys.exit(1)

            if lo == hi:
                print(f"NOTE: --bottom == --top == {lo:.0f} ns; that "
                      f"spacing passes -- dead time is at or below "
                      f"{lo:.0f} ns")
                sg.off()
                sys.exit(0)

            print("Verifying lower bound...")
            if test_spacing(sg, tic, lo, channel, ncyc=ncyc,
                            text=text, verbose=verbose):
                print(f"NOTE: Lower bound {lo:.0f} ns already passes "
                      f"-- decrease --bottom or dead time is below "
                      f"{lo:.0f} ns")
                sg.off()
                sys.exit(0)

            print()
            print("Binary search:")

            while (hi - lo) > args.precision:
                mid = (lo + hi) / 2
                mid = round(mid)
                if mid == lo or mid == hi:
                    break
                if test_spacing(sg, tic, mid, channel, ncyc=ncyc,
                                text=text, verbose=verbose):
                    hi = mid
                else:
                    lo = mid

            print()
            print(f"Dead time is between {lo:.0f} ns and {hi:.0f} ns")
            sg.off()
            print("Signal generator output off.")


if __name__ == "__main__":
    main()
