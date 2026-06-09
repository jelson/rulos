#!/usr/bin/env python3

"""End-to-end validation of the pulsegen, measured with a LectroTIC-4
timestamper.

Hardware setup this test assumes:
  - A pulsegen and a timestamper both on USB (autodetected by VID/PID).
  - Pulsegen ch0 -> timestamper ch0, pulsegen ch1 -> timestamper ch1.
  - Both devices fed from the same 10 MHz reference.

What it checks:
  - SYNC-mode inter-pulse gaps: places ch0 and ch1 rising edges a known
    distance apart (4 ns, 8 ns, 100 ns, 1 us) and confirms the timestamper
    measures that gap. Because ch0 (PC6) and ch1 (PC7) share HRTIM Timer F,
    this validates intra-timer delay placement; it does not exercise the
    cross-group master sync (that needs ch0+ch2 wiring), and it behaves the
    same in SYNC and ASYNC at these two outputs.
  - ASYNC frequency accuracy: drives a few frequencies and confirms the
    measured pulse rate.
  - ASYNC pairing constraint: ch0 and ch1 are forced to share Timer F's
    period, so setting two different frequencies leaves both at the last one.
    This asserts that documented behavior.

Caveats baked into the tolerances:
  - The two devices share the 10 MHz clock, so they're phase-coherent and the
    timestamper's 4 ns resolution can't finely resolve sub-2-tick gaps. The
    4 ns and 8 ns checks are coarse (+-1 tick); 100 ns and 1 us are tight.
  - 1 ms gaps aren't tested: the gap is a delay inside the shared period and
    HRTIM tops out at ~524 us. (TIM3 is on these pins and could reach longer
    periods, but the firmware is HRTIM-only for this board.)

Run:
  regression_test.py
  regression_test.py --only sync --duration 3
  regression_test.py --ts-port /dev/ttyACM0 --pg-port /dev/ttyACM2
"""

import argparse
import os
import statistics
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "util"))
sys.path.insert(0, os.path.join(HERE, "..", "..", "timestamper", "util"))

from pgctl import Pulsegen
from tsctl import LectroTIC4, Timestamp, PulsesLost, OscillatorFailure

NS = 1_000_000_000  # ns per second


# ---- Measurement helpers --------------------------------------------------

def collect(ts, duration_s, channels=(0, 1)):
    """Read the timestamp stream for duration_s and return a list of
    (channel, abs_ns) for the requested channels, abs_ns = seconds*1e9 +
    nanoseconds kept as an exact int (the library's seconds/nanoseconds are
    ints, so no float ever touches a timestamp).

    Raises immediately if the timestamper reports a lost pulse (overcapture or
    buffer overflow) or an oscillator failure -- a regression test must see
    every pulse, so any loss is a failure, not something to tolerate."""
    records = []
    for rec in ts.read_for(duration_s):
        if isinstance(rec, Timestamp):
            if rec.channel in channels:
                records.append((rec.channel, rec.seconds * NS + rec.nanoseconds))
        elif isinstance(rec, PulsesLost):
            if rec.overcaptures or rec.buf_overflows:
                raise RuntimeError(
                    f"timestamper lost pulses on ch{rec.channel}: "
                    f"{rec.overcaptures} overcaptures, "
                    f"{rec.buf_overflows} buffer overflows")
        elif isinstance(rec, OscillatorFailure):
            raise RuntimeError("timestamper reported oscillator failure")
    return records


def by_channel(records):
    """Split records into per-channel time lists, each sorted ascending. The
    device does NOT guarantee timestamps arrive in time order, so we sort."""
    out = {}
    for ch, t in records:
        out.setdefault(ch, []).append(t)
    for ch in out:
        out[ch].sort()
    return out


def zipper_gaps(records, period_ns):
    """Split by channel, sort each channel independently, and zipper the two
    sorted lists index-for-index: the i-th ch0 edge pairs with the i-th ch1
    edge (the i-th period, since ch0 fires gap-before ch1 every period).
    Return the list of (ch1 - ch0) gaps.

    The capture window does not begin or end on a clean period boundary, and
    the device delivers each channel in batches, so at each end one channel can
    lead the other by many edges. Those boundary edges have no partner -- they
    are window edges, not missed pulses -- so trim any orphan (a ch1 whose ch0
    is before the window, or a ch0 whose ch1 is after it) from both ends of
    both channels. A valid pair is gap-apart (< one period); an orphan is a
    whole period or more away.

    After trimming, detecting missed pulses is the point of the test, so we are
    strict: the two channels must have equal counts, every pair must fall
    within one period, and every period must be present (consecutive
    same-channel spacing stays near one period). Any of these failing means an
    interior pulse was dropped or duplicated."""
    by = by_channel(records)
    ch0 = by.get(0, [])
    ch1 = by.get(1, [])
    # Leading orphans: ch1 before the first ch0, or a ch0 whose partner ch1 is
    # a full period or more away (its ch1 fell before the window start).
    while ch1 and ch0 and ch1[0] < ch0[0]:
        ch1.pop(0)
    while ch0 and ch1 and ch1[0] - ch0[0] >= period_ns:
        ch0.pop(0)
    # Trailing orphans: ch0 after the last ch1, or a ch1 whose partner ch0 is a
    # full period or more before it (its ch0 fell after the window end).
    while ch0 and ch1 and ch0[-1] > ch1[-1]:
        ch0.pop()
    while ch1 and ch0 and ch1[-1] - ch0[-1] >= period_ns:
        ch1.pop()
    if len(ch0) != len(ch1):
        raise RuntimeError(
            f"channel pulse counts differ ({len(ch0)} vs {len(ch1)}) -- "
            f"a pulse was missed or duplicated")
    deltas = [b - a for a, b in zip(ch0, ch1)]
    if any(not (0 <= d < period_ns) for d in deltas):
        raise RuntimeError(
            "a ch0/ch1 pair does not fall within one period -- "
            "a pulse was missed or duplicated")
    for seq in (ch0, ch1):
        if any(seq[i + 1] - seq[i] > period_ns * 3 // 2
               for i in range(len(seq) - 1)):
            raise RuntimeError("a period is missing from the capture")
    return deltas


def median_interval(times):
    """Median spacing between consecutive edges (robust to occasional
    dropped pulses). None if fewer than 2 samples."""
    if len(times) < 2:
        return None
    diffs = [b - a for a, b in zip(times, times[1:])]
    return statistics.median(diffs)


def configure_ts(ts):
    """Defaults: rising-edge capture, divider 1, on both channels."""
    ts.reset()
    time.sleep(0.1)
    for ch in (0, 1):
        ts.set_slope(ch, "POS")
        ts.set_divider(ch, 1)


# ---- Tests ----------------------------------------------------------------

# Sync gap sweep parameters. 200 us period forces the pulsegen onto a 4 ns
# HRTIM tick (CKPSC 4), fine enough to place a 4 ns gap; the resulting
# 5 kHz/channel (10 k/s total) is far under the timestamper's sustained rate.
GAP_PERIOD_NS = 200_000
GAP_WIDTH_NS = GAP_PERIOD_NS // 4
GAP_BASE_NS = 1_000  # base delay on both channels, above the CKPSC-4 min
GAPS_NS = [4, 8, 100, 1000]


def gap_tolerance_ns(gap_ns):
    # +-1 timestamper tick for the sub-2-tick gaps (coherent-clock limit);
    # tighter for the larger gaps.
    if gap_ns <= 8:
        return 4.0
    return max(4.0, 2.0 + 0.01 * gap_ns)


def measure_gap(ts, pg, gap_ns, duration_s):
    """Configure ch0=base, ch1=base+gap (SYNC), measure mean(ch1-ch0)."""
    pg.set_mode(True)  # SYNC
    pg.set_period(0, GAP_PERIOD_NS / NS)  # sync: applies to all channels
    pg.set_width(0, GAP_WIDTH_NS / NS)
    pg.set_width(1, GAP_WIDTH_NS / NS)
    pg.set_delay(0, GAP_BASE_NS / NS)
    pg.set_delay(1, (GAP_BASE_NS + gap_ns) / NS)
    pg.set_state(0, True)
    pg.set_state(1, True)
    err = pg.get_error()
    if not err.startswith("0,"):
        raise RuntimeError(f"pulsegen error configuring gap {gap_ns} ns: {err}")

    time.sleep(0.1)
    ts.discard_pending()
    records = collect(ts, duration_s)
    deltas = zipper_gaps(records, GAP_PERIOD_NS)
    if len(deltas) < 50:
        raise RuntimeError(f"only {len(deltas)} zippered pairs (need >=50)")
    return statistics.fmean(deltas), min(deltas), max(deltas), len(deltas)


def test_sync_gaps(ts, pg, duration_s):
    print("\n=== SYNC inter-pulse gap sweep ===")
    print(f"  period {GAP_PERIOD_NS/1000:.0f} us, base delay {GAP_BASE_NS} ns")

    all_ok = True
    for gap in GAPS_NS:
        # min/max are raw per-sample gaps: exact integers, always multiples of
        # the 4 ns timestamper tick. avg is the mean, which resolves between
        # grid lines (the dither across min..max is what makes the average
        # meaningful below one tick). Any fixed channel skew is sub-tick here
        # (~0.01 ns) and absorbed by the tolerance.
        mean, mn, mx, n = measure_gap(ts, pg, gap, duration_s)
        err = mean - gap
        tol = gap_tolerance_ns(gap)
        ok = abs(err) <= tol
        all_ok = all_ok and ok
        print(f"  gap {gap:5d} ns: avg {mean:8.2f}  "
              f"min {mn:6d}  max {mx:6d}  "
              f"err {err:+6.2f}  tol +-{tol:.1f}  "
              f"({n} pairs)  {'PASS' if ok else 'FAIL'}")
    return all_ok


# ASYNC frequency sweep. Keep total reported rate modest via the timestamper
# divider so high pulsegen frequencies don't overrun the USB drain.
FREQS_HZ = [2_000, 10_000, 50_000, 200_000]


def measure_rate(ts, pg, freq_hz, duration_s):
    """Drive ch0/ch1 at freq_hz (ASYNC), return measured period (ns) on each
    channel. Uses a timestamper divider to keep the reported rate ~<=10 k/s."""
    period_ns = NS / freq_hz
    divider = max(1, round(freq_hz / 10_000))
    pg.set_mode(False)  # ASYNC
    pg.set_period(0, period_ns / NS)   # async: ch0 + its Timer-F sibling ch1
    pg.set_width(0, period_ns / 2 / NS)
    pg.set_width(1, period_ns / 2 / NS)
    pg.set_delay(0, 0)
    pg.set_delay(1, 0)
    pg.set_state(0, True)
    pg.set_state(1, True)
    err = pg.get_error()
    if not err.startswith("0,"):
        raise RuntimeError(f"pulsegen error at {freq_hz} Hz: {err}")

    for ch in (0, 1):
        ts.set_divider(ch, divider)
    time.sleep(0.1)
    ts.discard_pending()
    records = collect(ts, duration_s)
    ts.set_divider(0, 1)
    ts.set_divider(1, 1)
    times = by_channel(records)
    out = {}
    for ch in (0, 1):
        iv = median_interval(times.get(ch, []))
        if iv is None or len(times.get(ch, [])) < 50:
            raise RuntimeError(f"too few samples on ch{ch} at {freq_hz} Hz")
        out[ch] = iv / divider  # divider downsamples; scale back to true period
    return out, period_ns


def test_async_freq(ts, pg, duration_s):
    print("\n=== ASYNC frequency accuracy ===")
    all_ok = True
    for f in FREQS_HZ:
        measured, expect = measure_rate(ts, pg, f, duration_s)
        line_ok = True
        parts = []
        for ch in (0, 1):
            err_pct = 100.0 * (measured[ch] - expect) / expect
            ch_ok = abs(err_pct) <= 0.1
            line_ok = line_ok and ch_ok
            parts.append(f"ch{ch} {measured[ch]:.1f} ns ({err_pct:+.3f}%)")
        all_ok = all_ok and line_ok
        print(f"  {f:7d} Hz (expect {expect:.1f} ns): "
              f"{'  '.join(parts)}  {'PASS' if line_ok else 'FAIL'}")
    return all_ok


def test_async_pairing(ts, pg, duration_s):
    print("\n=== ASYNC pairing constraint (ch0/ch1 share Timer F) ===")
    f1, f2 = 10_000, 20_000  # set f1 then f2; both should end up at f2
    pg.set_mode(False)
    pg.set_period(0, 1 / f1)
    pg.set_period(1, 1 / f2)  # overrides the shared Timer-F period
    pg.set_width(0, 1 / f2 / 2)
    pg.set_width(1, 1 / f2 / 2)
    pg.set_delay(0, 0)
    pg.set_delay(1, 0)
    pg.set_state(0, True)
    pg.set_state(1, True)
    err = pg.get_error()
    if not err.startswith("0,"):
        raise RuntimeError(f"pulsegen error in pairing test: {err}")

    time.sleep(0.1)
    ts.discard_pending()
    records = collect(ts, duration_s)
    times = by_channel(records)
    expect_ns = NS / f2
    all_ok = True
    print(f"  set ch0={f1} Hz then ch1={f2} Hz; both should track {f2} Hz "
          f"({expect_ns:.1f} ns)")
    for ch in (0, 1):
        iv = median_interval(times.get(ch, []))
        if iv is None or len(times.get(ch, [])) < 50:
            raise RuntimeError(f"too few samples on ch{ch} in pairing test")
        err_pct = 100.0 * (iv - expect_ns) / expect_ns
        ok = abs(err_pct) <= 0.1
        all_ok = all_ok and ok
        print(f"  ch{ch}: {iv:.1f} ns ({err_pct:+.3f}%)  {'PASS' if ok else 'FAIL'}")
    return all_ok


# ---- Driver ---------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--ts-port", default=None,
                   help="Timestamper serial port (default: autodetect)")
    p.add_argument("--pg-port", default=None,
                   help="Pulsegen serial port (default: autodetect)")
    p.add_argument("--duration", type=float, default=2.0,
                   help="Capture seconds per measurement (default: 2.0)")
    p.add_argument("--only", choices=["sync", "async"], default=None,
                   help="Run only the sync or only the async tests")
    args = p.parse_args()

    results = {}
    with Pulsegen(port=args.pg_port) as pg, \
         LectroTIC4(port=args.ts_port) as ts:
        print(f"Pulsegen:    {pg.idn()}")
        print(f"Timestamper: {ts.idn()}")
        try:
            pg.reset()
            configure_ts(ts)

            if args.only != "async":
                results["sync_gaps"] = test_sync_gaps(ts, pg, args.duration)
            if args.only != "sync":
                results["async_freq"] = test_async_freq(ts, pg, args.duration)
                results["async_pairing"] = test_async_pairing(ts, pg, args.duration)
        finally:
            pg.off()
            pg.reset()
            ts.reset()

    print("\n=== Summary ===")
    for name, ok in results.items():
        print(f"  {name:16s} {'PASS' if ok else 'FAIL'}")
    all_ok = all(results.values())
    print(f"\n{'ALL PASS' if all_ok else 'FAILURES PRESENT'}")
    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
