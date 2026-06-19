#!/usr/bin/env python3

"""End-to-end validation of the pulsegen, measured with a LectroTIC-4
timestamper.

  *** REV B PORT IN PROGRESS -- the test BODIES below are still the Rev A
  versions and will not run as-is on Rev B. They are kept as a reference to
  port against once Rev B hardware exists; the header is the Rev B test plan.
  The Rev A board (PC6-9, paired HRTIM E/F) is retired (tag: pulsegen-revA). ***

REV B HARDWARE SETUP (when the board exists):
  - Pulsegen and timestamper both on USB (autodetected by VID/PID).
  - Pulsegen ch0..ch3 -> timestamper ch0..ch3 (now four independent channels;
    Rev A only wired two).
  - Both devices fed from the same 10 MHz reference.

REV B TEST PLAN -- what first silicon must validate (the new capabilities, and
the things that compile-clean but need hardware, per PCB_NOTES.md bring-up
checklist):
  1. FOUR INDEPENDENT PERIODS (the headline Rev B fix; replaces the obsolete
     Rev A "pairing constraint" test). Set four DIFFERENT frequencies in ASYNC
     and confirm each channel reads back its own -- proving the four distinct
     HRTIM/GP timers, no shared period.
  2. HRTIM<->GP HANDOFF continuity. Sweep one channel's period across the
     ~524 us crossover (e.g. 400 us, 524 us, 600 us, 1 ms) and confirm rate,
     width, and (in sync) delay stay correct on both sides -- the handoff must
     be invisible.
  3. LONG PERIODS (GP regime). Drive sub-1.9 kHz down toward ~0.03 Hz (34 s)
     and confirm the rate -- the capability Rev A lacked entirely.
  4. CROSS-TIMER SYNC. The 250 ps stair across all four channels, now on four
     distinct HRTIM timers (HRTIM master), and a slow stair in the GP regime
     (TIM1 master, ITR0) -- confirm phase-lock both ways (the Rev A v0.2.2
     cross-group sync work, generalized).
  5. BURST in both regimes. Fast (HRTIM BMC) and slow (GP software count) must
     each emit exactly N pulses per frame at the requested spacing; invalid
     params latch the documented SCPI errors; burst above ~524 us is rejected
     (burst is fast-regime only).
  6. PER-PIN AF. Drive every channel in both regimes and confirm all four emit
     (the Rev A Timer-E-silent bug was a wrong AF; Rev B AFs are AF13/AF13/
     AF13/AF3 fast and AF10/AF6/AF2/AF4 slow -- all per-pin).

Caveats (carried from Rev A, still apply):
  - The two devices share the 10 MHz clock, so sub-2-tick gaps resolve only to
    +-1 timestamper tick; wider gaps are tight.

Run (once ported):
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


def strict_interval(times, tol_ns=12):
    """Mean spacing between consecutive edges. Every individual gap
    must sit within tol_ns of the median gap: a dropped, duplicated,
    or misplaced pulse is a failure, never averaged away. None if
    fewer than 2 samples."""
    if len(times) < 2:
        return None
    diffs = [b - a for a, b in zip(times, times[1:])]
    med = statistics.median(diffs)
    worst = max(diffs, key=lambda d: abs(d - med))
    if abs(worst - med) > tol_ns:
        raise RuntimeError(
            f"a gap deviates {worst - med:+.1f} ns from the "
            f"{med:.1f} ns median -- a pulse was dropped, duplicated, "
            f"or misplaced")
    return sum(diffs) / len(diffs)


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
    pg.set_mode(Pulsegen.SYNC)
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

    ts.discard_pending(settle_s=0.1)
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
    pg.set_mode(Pulsegen.ASYNC)
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
    ts.discard_pending(settle_s=0.1)
    records = collect(ts, duration_s)
    ts.set_divider(0, 1)
    ts.set_divider(1, 1)
    times = by_channel(records)
    out = {}
    for ch in (0, 1):
        iv = strict_interval(times.get(ch, []))
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
    pg.set_mode(Pulsegen.ASYNC)
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

    ts.discard_pending(settle_s=0.1)
    records = collect(ts, duration_s)
    times = by_channel(records)
    expect_ns = NS / f2
    all_ok = True
    print(f"  set ch0={f1} Hz then ch1={f2} Hz; both should track {f2} Hz "
          f"({expect_ns:.1f} ns)")
    for ch in (0, 1):
        iv = strict_interval(times.get(ch, []))
        if iv is None or len(times.get(ch, [])) < 50:
            raise RuntimeError(f"too few samples on ch{ch} in pairing test")
        err_pct = 100.0 * (iv - expect_ns) / expect_ns
        ok = abs(err_pct) <= 0.1
        all_ok = all_ok and ok
        print(f"  ch{ch}: {iv:.1f} ns ({err_pct:+.3f}%)  {'PASS' if ok else 'FAIL'}")
    return all_ok


# Burst sweep: pulse counts at two scales. Counts and intra-burst spacing
# are hardware-exact; the repetition interval is software-scheduled on the
# device (one 10 ms scheduler jiffy of granularity), so its check is coarse
# and one-sided (a repetition fires on the first jiffy at or after its due
# time, never early).
BURST_CASES = [
    # (ncyc, spacing_ns, rep_s)
    (50, 10_000, 0.25),
    (1000, 1_000, 0.25),
]
BURST_SPACING_TOL_NS = 12
BURST_REP_EARLY_S = 0.005
BURST_REP_LATE_S = 0.060

# Invalid burst parameter -> the error code SYST:ERR? must latch. Each is
# applied to an otherwise-valid running burst.
BURST_ERROR_CASES = [
    ("SOUR0:BURS:NCYC 0", "-200"),
    ("SOUR0:BURS:NCYC 63489", "-200"),
    ("SOUR0:BURS:INT:PER 0.01", "-200"),
]


def group_bursts(times, gap_ns):
    """Split a sorted timestamp list into bursts: a gap longer than gap_ns
    starts a new burst."""
    bursts = []
    for t in times:
        if bursts and t - bursts[-1][-1] <= gap_ns:
            bursts[-1].append(t)
        else:
            bursts.append([t])
    return bursts


def measure_burst(ts, pg, ncyc, spacing_ns, rep_s, duration_s):
    """Run one burst configuration on ch0 and return (sizes, spacing_errs,
    rep_gaps): interior burst sizes, the worst intra-burst spacing error in
    ns, and the burst-to-burst start intervals in seconds. The capture
    window truncates the first and last burst, so only interior bursts
    count."""
    pg.burst(0, spacing_ns / NS, spacing_ns / 4 / NS, ncyc, rep_s)
    err = pg.get_error()
    if not err.startswith("0,"):
        raise RuntimeError(f"pulsegen error configuring burst: {err}")

    ts.discard_pending(settle_s=0.3)
    records = collect(ts, duration_s, channels=(0,))
    times = sorted(t for _, t in records)
    bursts = group_bursts(times, int(rep_s * NS / 2))
    if len(bursts) < 4:
        raise RuntimeError(f"only {len(bursts)} bursts captured "
                           f"(need >=4 to have interior bursts)")
    interior = bursts[1:-1]
    sizes = [len(b) for b in interior]
    worst_err = max((abs((b - a) - spacing_ns)
                     for burst in interior
                     for a, b in zip(burst, burst[1:])), default=0)
    rep_gaps = [(b[0] - a[0]) / NS for a, b in zip(interior, interior[1:])]
    return sizes, worst_err, rep_gaps


def test_burst(ts, pg, duration_s):
    print("\n=== Burst mode (hardware-exact N-cycle bursts) ===")
    duration_s = max(duration_s, 2.0)  # need >=4 bursts at the 0.25 s rep
    all_ok = True

    for ncyc, spacing_ns, rep_s in BURST_CASES:
        sizes, worst_err, rep_gaps = measure_burst(
            ts, pg, ncyc, spacing_ns, rep_s, duration_s)
        count_ok = all(s == ncyc for s in sizes)
        spacing_ok = worst_err <= BURST_SPACING_TOL_NS
        rep_ok = all(rep_s - BURST_REP_EARLY_S <= g <= rep_s + BURST_REP_LATE_S
                     for g in rep_gaps)
        ok = count_ok and spacing_ok and rep_ok
        all_ok = all_ok and ok
        print(f"  ncyc {ncyc:5d} @ {spacing_ns} ns, rep {rep_s} s: "
              f"counts {min(sizes)}..{max(sizes)} ({len(sizes)} bursts)  "
              f"spacing err {worst_err:.0f} ns  "
              f"rep {min(rep_gaps):.3f}..{max(rep_gaps):.3f} s  "
              f"{'PASS' if ok else 'FAIL'}")

    # Error latching: each invalid parameter against a running burst.
    for cmd, want_code in BURST_ERROR_CASES:
        pg.burst(0, 1e-5, 2.5e-6, 50, 0.25)
        pg.send(cmd)
        err = pg.get_error()
        ok = err.startswith(want_code)
        all_ok = all_ok and ok
        print(f"  reject [{cmd}]: {err}  {'PASS' if ok else 'FAIL'}")

    # Disabling a live burst fails safe: the domain's outputs turn OFF
    # rather than running continuously (the domain period is the
    # intra-burst spacing, so running on would stream at the burst's
    # instantaneous rate -- a 48 ns burst would become ~20 MHz).
    pg.pulse_burst(0, 10_000, 50)
    pg.set_burst_state(0, False)
    ts.discard_pending(settle_s=0.2)
    records = collect(ts, 1.0, channels=(0,))
    ok = len(records) == 0
    all_ok = all_ok and ok
    print(f"  burst off -> outputs off: {len(records)} pulses in 1 s "
          f"(expect 0)  {'PASS' if ok else 'FAIL'}")

    # Reconfiguring through pulse() does return the channel to a
    # continuous train (it clears burst state and re-enables the
    # output): expect far more pulses than any burst could deliver.
    pg.set_burst_ncycles(0, 50)
    pg.set_burst_period(0, 0.25)
    pg.pulse(0, 1e-5, 2.5e-6)  # clears burst state, re-enables output
    ts.discard_pending(settle_s=0.2)
    records = collect(ts, 1.0, channels=(0,))
    expect_min = int(0.5 * NS / 10_000)  # half the window at 10 us periods
    ok = len(records) >= expect_min
    all_ok = all_ok and ok
    print(f"  pulse() after burst -> continuous: {len(records)} pulses "
          f"in 1 s (>= {expect_min})  {'PASS' if ok else 'FAIL'}")
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
    p.add_argument("--only", choices=["sync", "async", "burst"], default=None,
                   help="Run only one test group")
    args = p.parse_args()

    results = {}
    with Pulsegen(port=args.pg_port) as pg, \
         LectroTIC4(port=args.ts_port) as ts:
        print(f"Pulsegen:    {pg.idn()}")
        print(f"Timestamper: {ts.idn()}")
        try:
            pg.reset()
            configure_ts(ts)

            if args.only in (None, "sync"):
                results["sync_gaps"] = test_sync_gaps(ts, pg, args.duration)
            if args.only in (None, "async"):
                results["async_freq"] = test_async_freq(ts, pg, args.duration)
                results["async_pairing"] = test_async_pairing(ts, pg, args.duration)
            if args.only in (None, "burst"):
                results["burst"] = test_burst(ts, pg, args.duration)
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
