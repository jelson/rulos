#!/usr/bin/env python3

"""Binary-search for the LectroTIC-4's sustained USB throughput.

Drives a continuous pulse train at increasing frequencies and verifies
that every pulse makes it out of the LectroTIC-4 to the host. This
measures USB drain bandwidth, not internal capture (deadtime.py covers
the bursty regime where the on-device buffer absorbs short overruns).

For each frequency we hold the port open, read-and-discard for --settle
seconds, then verify every gap between consecutive timestamps in the
following --measure window is within --tolerance of the expected period
(1/hz). We don't count totals — a slightly late start can chop a few
samples off without indicating a real failure — but a single bad gap
fails the test.

Usage:
  sustained_rate.py                            defaults: 1000-100000 Hz
  sustained_rate.py --bottom 5000 --top 200000 custom range in Hz
  sustained_rate.py --measure 10               longer measurement window
  sustained_rate.py --port /dev/ttyACM1        custom serial port
  sustained_rate.py --host siggen2             custom signal generator host
"""

import argparse
import sys

from tsctl import LectroTIC4, record_seconds
from siggen import Siggen


def measure_rate(tic, settle_s, measure_s, verbose):
    """Discard records for settle_s seconds, then collect timestamps
    for measure_s. Returns (timestamps_float_seconds, overflow_count)."""
    # Configuring siggen takes a few hundred ms (output_off, set the
    # rate, output_on, run verify queries); during that time the ring
    # may have filled and lost records. Drop everything pending so the
    # measurement window starts clean.
    tic.discard_pending()

    timestamps = []
    overflows = 0
    for r in tic.read_for(measure_s):
        if r.kind == "comment":
            if "overflow" in r.comment.lower():
                overflows += 1
            if verbose:
                print(f"    | {r.comment}")
            continue
        timestamps.append(record_seconds(r))
    return timestamps, overflows


def test_rate(sg, tic, hz, settle_s, measure_s, tolerance, verbose):
    """Configure siggen for `hz` Hz continuous, measure, and decide
    pass/fail. Returns True on pass."""
    period_s = 1.0 / hz
    pulse_width_s = min(200e-9, period_s / 4)

    print(f"  Testing {hz:>7.0f} Hz "
          f"(pulse width {pulse_width_s * 1e9:.0f} ns)...")
    sg.periodic(hz, pulse_width_s=pulse_width_s)

    timestamps, overflows = measure_rate(tic, settle_s, measure_s, verbose)

    deltas = [timestamps[i] - timestamps[i - 1]
              for i in range(1, len(timestamps))]
    bad_gaps = sum(1 for d in deltas if abs(d - period_s) > tolerance)

    passed = (overflows == 0) and (bad_gaps == 0) and (len(timestamps) > 1)

    if deltas:
        worst = max(deltas, key=lambda d: abs(d - period_s))
        worst_err_us = (worst - period_s) * 1e6
    else:
        worst_err_us = float("nan")

    status = "PASS" if passed else "FAIL"
    print(f"  {hz:>7.0f} Hz: {status}  "
          f"n={len(timestamps)} "
          f"bad_gaps={bad_gaps}/{len(deltas)} "
          f"worst_dev={worst_err_us:+.2f} µs "
          f"overflows={overflows}")
    return passed


def main():
    p = argparse.ArgumentParser(
        description="Binary-search for sustained LectroTIC-4 USB throughput",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--bottom", type=float, default=1000,
                   help="Lower bound in Hz (default: 1000)")
    p.add_argument("--top", type=float, default=100000,
                   help="Upper bound in Hz (default: 100000)")
    p.add_argument("--port", default=None,
                   help="LectroTIC-4 serial port (default: autodetect)")
    p.add_argument("--host", default="siggen",
                   help="Signal generator hostname (default: siggen)")
    p.add_argument("--precision", type=float, default=500,
                   help="Stop when range narrows to this many Hz "
                        "(default: 500)")
    p.add_argument("--settle", type=float, default=2.0,
                   help="Seconds to discard before measuring (default: 2)")
    p.add_argument("--measure", type=float, default=5.0,
                   help="Seconds to measure each frequency (default: 5)")
    p.add_argument("--tolerance", type=float, default=1e-6,
                   help="Per-gap tolerance in seconds. A frequency passes "
                        "only if every gap is within this much of 1/hz "
                        "(default: 1e-6 = 1 µs)")
    p.add_argument("--text", action="store_true",
                   help="Use TEXT wire format instead of BINARY (default)")
    p.add_argument("--quiet", "-q", action="store_true",
                   help="Suppress raw serial overflow output")
    args = p.parse_args()

    verbose = not args.quiet
    binary = not args.text
    lo = args.bottom
    hi = args.top

    with LectroTIC4(args.port) as tic:
        print(f"Searching for max sustained rate in range "
              f"[{lo:.0f}, {hi:.0f}] Hz")
        print(f"LectroTIC-4 port: {tic.port}")
        print(f"Signal generator: {args.host}")
        print(f"Wire format:      {'BINARY' if binary else 'TEXT'}")
        print(f"Measurement: {args.measure}s after {args.settle}s settle, "
              f"per-gap tolerance ±{args.tolerance * 1e6:.2f} µs")
        print()

        tic.reset()
        tic.reset_input_buffer()
        tic.set_binary(binary)
        try:
            with Siggen(host=args.host) as sg:
                print(f"Connected to: {sg.idn()}")
                print()

                print("Verifying lower bound passes...")
                if not test_rate(sg, tic, lo, args.settle, args.measure,
                                 args.tolerance, verbose):
                    print(f"ERROR: Lower bound {lo:.0f} Hz fails -- "
                          f"decrease --bottom")
                    sg.output_off()
                    sys.exit(1)

                print("Verifying upper bound fails...")
                if test_rate(sg, tic, hi, args.settle, args.measure,
                             args.tolerance, verbose):
                    print(f"NOTE: Upper bound {hi:.0f} Hz already passes -- "
                          f"increase --top to find the ceiling")
                    sg.output_off()
                    sys.exit(0)

                print()
                print("Binary search:")
                while (hi - lo) > args.precision:
                    mid = round((lo + hi) / 2)
                    if mid == lo or mid == hi:
                        break
                    if test_rate(sg, tic, mid, args.settle, args.measure,
                                 args.tolerance, verbose):
                        lo = mid
                    else:
                        hi = mid

                print()
                print(f"Sustained rate is between {lo:.0f} Hz and {hi:.0f} Hz")
                sg.output_off()
                print("Signal generator output off.")
        finally:
            # Restore TEXT for downstream tools.
            tic.set_binary(False)


if __name__ == "__main__":
    main()
