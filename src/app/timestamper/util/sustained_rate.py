#!/usr/bin/env python3

"""Binary-search for the LectroTIC-4's sustained USB throughput.

Drives a continuous pulse train at increasing frequencies and verifies
that every pulse makes it out of the LectroTIC-4 to the host. This
measures USB drain bandwidth, not internal capture (deadtime.py covers
the bursty regime where the on-device buffer absorbs short overruns).

For each frequency we reconfigure the siggen, then drain the
post-reconfigure residue for --settle seconds with the new signal
already running (transition isolation: a frequency change leaves
DMA-capture/ring residue from the old rate that the firmware streams
out after the marker sync — a one-time transient, not a throughput
deficit; the same isolation regression_test.phase_sustained does).
Only then do we measure: over the --measure window require (a) the
device reported no loss — no PULSES_LOST records — and (b) every
*interior* gap between consecutive timestamps is within --tolerance
of the expected period (1/hz). The first/last gap touches the capture
window edge, where a record can be clipped or torn — excluded, as
regression_test.phase_sustained does. We don't count totals — a
slightly late start can chop a few samples without indicating a real
failure — but one lost pulse or one bad interior gap fails the
frequency.

Usage:
  sustained_rate.py                            defaults: 1000-100000 Hz
  sustained_rate.py --bottom 5000 --top 200000 custom range in Hz
  sustained_rate.py --measure 10               longer measurement window
  sustained_rate.py --port /dev/ttyACM1        custom serial port
  sustained_rate.py --host siggen2             custom signal generator host
"""

import argparse
import sys

from tsctl import LectroTIC4, Timestamp, PulsesLost, OscillatorFailure
from siggen import Siggen
from regression_test import parse_text_stream


def measure_rate(tic, measure_s, text=False, settle_s=0.5):
    """Drain the post-reconfigure residue for settle_s with the new
    signal already running, then collect the stream for measure_s.
    Returns (timestamps_float_seconds, lost_count). lost_count sums the
    overcaptures + buffer overflows the device reports -- in BINARY
    mode (default) via the in-band PULSES_LOST records, with text=True
    via the ASCII diagnostic lines (both the definitive no-loss
    signal). The two formats have different USB-drain ceilings.

    The settle drain is transition isolation, mirroring
    regression_test.phase_sustained: a siggen frequency change leaves
    old-rate DMA-capture/ring residue that the firmware streams out
    after the marker sync. Measuring it as a gap misreads a one-time
    reconfiguration transient as a sustained-rate failure (a single
    multi-ms gap, lost=0). discard -> drain through the residue with
    the new rate running -> discard -> measure clean."""
    if text:
        tic.send("FORM:DATA TEXT")
        tic.set_stream_enabled(True)
        tic.discard_pending()
        tic.read_raw(settle_s)   # drain old-rate residue
        tic.discard_pending()    # clean measurement start
        timestamps, _others, overcaptures, overflows, _comments = \
            parse_text_stream(tic.read_raw(measure_s), channel=0)
        return timestamps, overcaptures + overflows

    tic.discard_pending()
    tic.read_raw(settle_s)   # drain old-rate residue
    tic.discard_pending()    # clean measurement start
    timestamps = []
    lost = 0
    for r in tic.read_for(measure_s):
        if isinstance(r, Timestamp):
            timestamps.append(r.seconds + r.nanoseconds * 1e-9)
        elif isinstance(r, PulsesLost):
            lost += r.overcaptures + r.buf_overflows
        elif isinstance(r, OscillatorFailure):
            raise RuntimeError("reference clock failed mid-measurement "
                               "-- device halted; restore the 10 MHz "
                               "source and reset")
    return timestamps, lost


def test_rate(sg, tic, hz, measure_s, tolerance, text=False, settle_s=0.5):
    """Configure siggen for `hz` Hz continuous, measure, and decide
    pass/fail. Returns True on pass."""
    period_s = 1.0 / hz
    pulse_width_s = min(200e-9, period_s / 4)

    print(f"  Testing {hz:>7.0f} Hz "
          f"(pulse width {pulse_width_s * 1e9:.0f} ns)...")
    sg.periodic(hz, pulse_width_s=pulse_width_s)

    timestamps, lost = measure_rate(tic, measure_s, text=text,
                                    settle_s=settle_s)

    deltas = [timestamps[i] - timestamps[i - 1]
              for i in range(1, len(timestamps))]
    # Score interior gaps only, mirroring regression_test.phase_sustained:
    # the first/last gap touches the capture-window edge, where a record
    # can be clipped or torn (a window cut mid-record, or a seconds
    # rollover straddling the boundary) -- a window artifact, not a
    # device miss. A real miss is interior and a >=1*period excursion.
    interior = deltas[1:-1]
    bad_gaps = sum(1 for d in interior if abs(d - period_s) > tolerance)

    passed = (lost == 0) and (bad_gaps == 0) and (len(interior) > 0)

    if interior:
        worst = max(interior, key=lambda d: abs(d - period_s))
        worst_err_ns = (worst - period_s) * 1e9
    else:
        worst_err_ns = float("nan")

    status = "PASS" if passed else "FAIL"
    print(f"  {hz:>7.0f} Hz: {status}  "
          f"n={len(timestamps)} "
          f"lost={lost} "
          f"bad_gaps={bad_gaps}/{len(interior)} "
          f"worst_dev={worst_err_ns:+.0f} ns")
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
    p.add_argument("--measure", type=float, default=5.0,
                   help="Seconds to measure each frequency (default: 5)")
    # The siggen and device run off the same 10 MHz lab standard (no
    # drift between them) and the device timestamps at 4 ns, so a clean
    # gap is the period +/- a few ns; a dropped pulse is a whole-period
    # excursion. 12 ns matches regression_test.py's GAP_TOL_S.
    p.add_argument("--tolerance", type=float, default=12e-9,
                   help="Per-gap tolerance in seconds; a frequency "
                        "passes only if every gap is within this of "
                        "1/hz (default: 12e-9 = 12 ns)")
    p.add_argument("--text", action="store_true",
                   help="Measure the ASCII TEXT stream instead of the "
                        "BINARY union (different USB-drain ceiling)")
    p.add_argument("--settle", type=float, default=0.5,
                   help="Seconds to drain post-reconfigure residue "
                        "before each measurement window; transition "
                        "isolation so a one-time siggen-reconfig "
                        "transient is not misread as a rate failure "
                        "(default: 0.5, matching phase_sustained)")
    args = p.parse_args()

    lo = args.bottom
    hi = args.top
    text = args.text
    settle = args.settle

    with LectroTIC4(args.port) as tic:
        print(f"Searching for max sustained rate in range "
              f"[{lo:.0f}, {hi:.0f}] Hz")
        print(f"LectroTIC-4 port: {tic.port}")
        print(f"Signal generator: {args.host}")
        print(f"Wire format: {'TEXT' if text else 'BINARY'}")
        print(f"Measurement: {args.measure}s/frequency, "
              f"{settle:.1f}s settle, per-gap "
              f"tolerance +/-{args.tolerance * 1e9:.0f} ns, "
              f"loss via in-band PULSES_LOST")
        print()

        with Siggen(host=args.host) as sg:
            print(f"Connected to: {sg.idn()}")
            print()

            print("Verifying lower bound passes...")
            if not test_rate(sg, tic, lo, args.measure, args.tolerance,
                             text=text):
                print(f"ERROR: Lower bound {lo:.0f} Hz fails -- "
                      f"decrease --bottom")
                sg.output_off()
                sys.exit(1)

            print("Verifying upper bound fails...")
            if test_rate(sg, tic, hi, args.measure, args.tolerance,
                         text=text):
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
                if test_rate(sg, tic, mid, args.measure, args.tolerance,
                             text=text):
                    lo = mid
                else:
                    hi = mid

            print()
            print(f"Sustained rate is between {lo:.0f} Hz and {hi:.0f} Hz")
            sg.output_off()
            print("Signal generator output off.")


if __name__ == "__main__":
    main()
