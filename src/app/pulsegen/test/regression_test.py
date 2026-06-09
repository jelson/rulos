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
import bisect
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
    """Stream timestamps for duration_s and bucket absolute-ns times per
    channel. Returns (buckets, lost) where buckets maps channel -> sorted
    list of ints (seconds*1e9 + nanoseconds), and lost is the total
    overcaptures+overflows reported. Raises RuntimeError on oscillator loss."""
    buckets = {c: [] for c in channels}
    lost = 0
    for rec in ts.read_for(duration_s):
        if isinstance(rec, Timestamp):
            if rec.channel in buckets:
                buckets[rec.channel].append(rec.seconds * NS + rec.nanoseconds)
        elif isinstance(rec, PulsesLost):
            lost += rec.overcaptures + rec.buf_overflows
        elif isinstance(rec, OscillatorFailure):
            raise RuntimeError("timestamper reported oscillator failure")
    return buckets, lost


def pair_deltas(ch0, ch1, period_ns):
    """For each ch0 edge, find the nearest ch1 edge; keep pairs within
    +-period/2. Returns the list of (ch1 - ch0) deltas in ns. Robust to
    dropped pulses (unmatched edges are skipped)."""
    deltas = []
    half = period_ns / 2
    for t0 in ch0:
        i = bisect.bisect_left(ch1, t0)
        best = None
        for j in (i - 1, i):
            if 0 <= j < len(ch1):
                d = ch1[j] - t0
                if best is None or abs(d) < abs(best):
                    best = d
        if best is not None and abs(best) <= half:
            deltas.append(best)
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
    buckets, lost = collect(ts, duration_s)
    if lost:
        raise RuntimeError(f"timestamper lost {lost} pulses during gap measurement")
    deltas = pair_deltas(buckets[0], buckets[1], GAP_PERIOD_NS)
    if len(deltas) < 50:
        raise RuntimeError(f"only {len(deltas)} paired edges (need >=50)")
    return statistics.fmean(deltas), len(deltas)


def test_sync_gaps(ts, pg, duration_s):
    print("\n=== SYNC inter-pulse gap sweep ===")
    print(f"  period {GAP_PERIOD_NS/1000:.0f} us, base delay {GAP_BASE_NS} ns")

    # Baseline (gap 0) measures the fixed channel/cable skew, subtracted out.
    skew, n = measure_gap(ts, pg, 0, duration_s)
    print(f"  baseline skew (gap 0): {skew:+.2f} ns  ({n} pairs)")

    all_ok = True
    for gap in GAPS_NS:
        measured, n = measure_gap(ts, pg, gap, duration_s)
        observed = measured - skew
        err = observed - gap
        tol = gap_tolerance_ns(gap)
        ok = abs(err) <= tol
        all_ok = all_ok and ok
        print(f"  gap {gap:5d} ns: measured {observed:8.2f} ns  "
              f"err {err:+6.2f} ns  tol +-{tol:.1f}  "
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
    buckets, lost = collect(ts, duration_s)
    ts.set_divider(0, 1)
    ts.set_divider(1, 1)
    if lost:
        raise RuntimeError(f"timestamper lost {lost} pulses at {freq_hz} Hz")

    out = {}
    for ch in (0, 1):
        iv = median_interval(buckets[ch])
        if iv is None or len(buckets[ch]) < 50:
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
    buckets, lost = collect(ts, duration_s)
    if lost:
        raise RuntimeError(f"timestamper lost {lost} pulses in pairing test")

    expect_ns = NS / f2
    all_ok = True
    print(f"  set ch0={f1} Hz then ch1={f2} Hz; both should track {f2} Hz "
          f"({expect_ns:.1f} ns)")
    for ch in (0, 1):
        iv = median_interval(buckets[ch])
        if iv is None or len(buckets[ch]) < 50:
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
