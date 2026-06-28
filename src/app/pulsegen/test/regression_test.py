#!/usr/bin/env python3

"""End-to-end validation of the PG-4 (Rev B) pulsegen, measured with a
LectroTIC-4 timestamper.

HARDWARE SETUP:
  - Pulsegen and timestamper both on USB (autodetected by VID/PID).
  - Pulsegen ch0..ch3 -> timestamper ch0..ch3 (four independent channels).
  - Both devices fed from the same 10 MHz reference.

WHAT THIS VALIDATES (the Rev B capabilities, and the things that compile-clean
but need hardware, per PCB_NOTES.md bring-up checklist):
  1. FOUR INDEPENDENT PERIODS. Four DIFFERENT frequencies in ASYNC, each
     channel reading back its own -- proving four distinct HRTIM/GP timers with
     no shared period (the headline Rev B fix; Rev A could only pair channels).
  2. HRTIM<->GP HANDOFF. Sweep one channel's period across the ~524 us
     crossover (400 us, 524 us, 600 us, 1 ms) and confirm rate AND width stay
     correct on both sides -- the handoff (and the GP-side combined-PWM pulse)
     must be invisible.
  3. LONG PERIODS (GP regime). Drive into the GP regime (down to a few Hz) and
     confirm the rate -- the slow capability Rev A lacked entirely.
  4. CROSS-TIMER SYNC. A fine stair across all four channels in the HRTIM
     regime (four distinct timers, HRTIM master) and a coarse stair in the GP
     regime (TIM1 master, ITR0) -- confirm every channel phase-locks to ch0.
  5. BURST in BOTH regimes. Fast (HRTIM burst-mode controller) and slow (GP
     software count) each emit exactly N pulses per frame at the requested
     spacing; the repetition interval is hardware-timed (TIM5), so its check is
     two-sided and tight; invalid params latch the documented SCPI errors.
  6. PER-PIN AF. Drive every channel in both regimes and confirm all four emit
     (the Rev A Timer-E-silent bug was a wrong AF; Rev B AFs are per-pin,
     AF13/AF13/AF13/AF3 fast and AF10/AF6/AF2/AF4 slow).

Caveats:
  - The two devices share the 10 MHz clock, so sub-2-tick gaps resolve only to
    +-1 timestamper tick (4 ns); wider gaps are tight.

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
NUM_CHANNELS = 4
CROSSOVER_NS = 524_000  # ~ the HRTIM period ceiling; above it a channel is on its GP timer


# ---- Measurement helpers --------------------------------------------------


def collect(ts, duration_s, channels=range(NUM_CHANNELS)):
    """Read the timestamp stream for duration_s and return a list of
    (channel, abs_ns, polarity) for the requested channels. abs_ns =
    seconds*1e9 + nanoseconds, kept as an exact int (the library's
    seconds/nanoseconds are ints, so no float ever touches a timestamp);
    polarity is '+' (rising) or '-' (falling).

    Raises immediately if the timestamper reports a lost pulse (overcapture or
    buffer overflow) or an oscillator failure -- a regression test must see
    every pulse, so any loss is a failure, not something to tolerate."""
    channels = set(channels)
    records = []
    for rec in ts.read_for(duration_s):
        if isinstance(rec, Timestamp):
            if rec.channel in channels:
                records.append((rec.channel, rec.seconds * NS + rec.nanoseconds, rec.polarity))
        elif isinstance(rec, PulsesLost):
            if rec.overcaptures or rec.buf_overflows:
                raise RuntimeError(
                    f"timestamper lost pulses on ch{rec.channel}: "
                    f"{rec.overcaptures} overcaptures, "
                    f"{rec.buf_overflows} buffer overflows"
                )
        elif isinstance(rec, OscillatorFailure):
            raise RuntimeError("timestamper reported oscillator failure")
    return records


def by_channel(records, polarity="+"):
    """Split records of the given polarity into per-channel time lists, each
    sorted ascending. The device does NOT guarantee timestamps arrive in time
    order, so we sort."""
    out = {}
    for ch, t, pol in records:
        if pol == polarity:
            out.setdefault(ch, []).append(t)
    for ch in out:
        out[ch].sort()
    return out


def strict_interval(times, tol_ns=12):
    """Mean spacing between consecutive edges. Every individual gap must sit
    within tol_ns of the median gap: a dropped, duplicated, or misplaced pulse
    is a failure, never averaged away. None if fewer than 2 samples."""
    if len(times) < 2:
        return None
    diffs = [b - a for a, b in zip(times, times[1:])]
    med = statistics.median(diffs)
    worst = max(diffs, key=lambda d: abs(d - med))
    if abs(worst - med) > tol_ns:
        raise RuntimeError(
            f"a gap deviates {worst - med:+.1f} ns from the "
            f"{med:.1f} ns median -- a pulse was dropped, duplicated, "
            f"or misplaced"
        )
    return sum(diffs) / len(diffs)


def zipper_pair(ref, other, period_ns):
    """Pair two sorted edge lists index-for-index: the i-th `other` edge pairs
    with the i-th `ref` edge (each period, `other` fires gap-after `ref`, with
    0 <= gap < one period). Return the list of (other - ref) gaps.

    The capture window does not begin or end on a clean period boundary, and
    the device delivers each channel in batches, so at each end one channel can
    lead the other by many edges. Those boundary edges have no partner -- window
    edges, not missed pulses -- so trim any orphan (an `other` whose `ref` is
    before the window, or a `ref` whose `other` is after it) from both ends.

    After trimming we are strict: equal counts, every pair within one period,
    and every period present (consecutive same-channel spacing near one period).
    Any failing means an interior pulse was dropped or duplicated."""
    ref = list(ref)
    other = list(other)
    while other and ref and other[0] < ref[0]:
        other.pop(0)
    while ref and other and other[0] - ref[0] >= period_ns:
        ref.pop(0)
    while ref and other and ref[-1] > other[-1]:
        ref.pop()
    while other and ref and other[-1] - ref[-1] >= period_ns:
        other.pop()
    if len(ref) != len(other):
        raise RuntimeError(
            f"channel pulse counts differ ({len(ref)} vs {len(other)}) -- "
            f"a pulse was missed or duplicated"
        )
    deltas = [b - a for a, b in zip(ref, other)]
    if any(not (0 <= d < period_ns) for d in deltas):
        raise RuntimeError(
            "a channel pair does not fall within one period -- a pulse was missed or duplicated"
        )
    for seq in (ref, other):
        if any(seq[i + 1] - seq[i] > period_ns * 3 // 2 for i in range(len(seq) - 1)):
            raise RuntimeError("a period is missing from the capture")
    return deltas


def pulse_widths(records, channel):
    """Per-pulse high time (ns) on `channel` from a both-edges capture: pair
    each rising edge with the next falling edge. Records must include both
    polarities (set the channel's slope to BOTH first)."""
    edges = sorted((t, pol) for ch, t, pol in records if ch == channel)
    widths = []
    rising = None
    for t, pol in edges:
        if pol == "+":
            rising = t
        elif rising is not None:
            widths.append(t - rising)
            rising = None
    return widths


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


def divider_for(freq_hz, target_rate=7000):
    """Timestamper divider that keeps a channel's reported rate near
    target_rate, so four channels together stay under the device budget."""
    return max(1, round(freq_hz / target_rate))


def configure_ts(ts, channels=range(NUM_CHANNELS)):
    """Defaults: rising-edge capture, divider 1, on every channel."""
    ts.reset()
    time.sleep(0.1)
    for ch in channels:
        ts.set_slope(ch, "POS")
        ts.set_divider(ch, 1)


def measure_periods(ts, pg, setpoints, duration_s):
    """Measure the period (ns) on each channel in `setpoints` ({ch: freq_hz}),
    using a per-channel timestamper divider sized to the setpoint. Returns
    {ch: measured_period_ns}. Strict: raises if any channel drops a pulse or
    yields too few samples."""
    div = {ch: divider_for(f) for ch, f in setpoints.items()}
    for ch, d in div.items():
        ts.set_divider(ch, d)
    ts.discard_pending(settle_s=0.1)
    records = collect(ts, duration_s, channels=setpoints)
    for ch in setpoints:
        ts.set_divider(ch, 1)
    times = by_channel(records)
    out = {}
    for ch in setpoints:
        seq = times.get(ch, [])
        iv = strict_interval(seq)
        if iv is None or len(seq) < 50:
            raise RuntimeError(f"too few samples on ch{ch} ({len(seq)}; need >=50)")
        out[ch] = iv / div[ch]  # divider downsamples; scale back to the true period
    return out


# ---- Test 1: four independent periods (ASYNC) -----------------------------

# Four distinct frequencies, all in the HRTIM regime, deliberately not multiples
# of one another: if any two channels shared a timer, at least one would read
# back the wrong period.
INDEP_FREQS = {0: 3_000, 1: 17_000, 2: 61_000, 3: 150_000}


def test_async_independent(ts, pg, duration_s):
    print("\n=== ASYNC four independent periods ===")
    pg.set_mode(Pulsegen.ASYNC)
    for ch, f in INDEP_FREQS.items():
        period_s = 1.0 / f
        pg.set_period(ch, period_s)
        pg.set_width(ch, period_s / 2)
        pg.set_delay(ch, 0)
        pg.set_state(ch, True)
    err = pg.get_error()
    if not err.startswith("0,"):
        raise RuntimeError(f"pulsegen error configuring independent periods: {err}")

    measured = measure_periods(ts, pg, INDEP_FREQS, duration_s)
    all_ok = True
    for ch, f in INDEP_FREQS.items():
        expect = NS / f
        err_pct = 100.0 * (measured[ch] - expect) / expect
        ok = abs(err_pct) <= 0.1
        all_ok = all_ok and ok
        print(
            f"  ch{ch}: set {f:7d} Hz (expect {expect:9.1f} ns) -> "
            f"{measured[ch]:9.1f} ns ({err_pct:+.3f}%)  {'PASS' if ok else 'FAIL'}"
        )
    return all_ok


# ---- Test 2: HRTIM <-> GP handoff continuity ------------------------------

# Periods straddling the ~524 us crossover: two HRTIM-side, the boundary, two
# GP-side. Rate and width must be continuous across the handoff.
HANDOFF_PERIODS_NS = [400_000, 524_000, 600_000, 1_000_000]
HANDOFF_DUTY = 0.25  # width = period/4


def test_handoff(ts, pg, duration_s):
    print("\n=== HRTIM<->GP handoff (rate + width across ~524 us) ===")
    duration_s = max(duration_s, 2.0)  # slow rates: give each period enough samples
    pg.set_mode(Pulsegen.ASYNC)
    ts.set_slope(0, "BOTH")  # capture both edges so we can measure width
    all_ok = True
    try:
        for period_ns in HANDOFF_PERIODS_NS:
            width_ns = period_ns * HANDOFF_DUTY
            pg.pulse(0, period_ns / NS, width_ns / NS)
            err = pg.get_error()
            if not err.startswith("0,"):
                raise RuntimeError(f"pulsegen error at {period_ns} ns: {err}")
            ts.discard_pending(settle_s=0.2)
            records = collect(ts, duration_s, channels=(0,))

            rising = by_channel(records).get(0, [])
            meas_period = strict_interval(rising, tol_ns=40)
            widths = pulse_widths(records, 0)
            if meas_period is None or len(widths) < 20:
                raise RuntimeError(f"too few samples at {period_ns} ns")
            meas_width = statistics.median(widths)

            regime = "HRTIM" if period_ns <= CROSSOVER_NS else "GP"
            p_err = 100.0 * (meas_period - period_ns) / period_ns
            w_err_ns = meas_width - width_ns
            ok = abs(p_err) <= 0.1 and abs(w_err_ns) <= max(20.0, 0.02 * width_ns)
            all_ok = all_ok and ok
            print(
                f"  {period_ns/1000:6.0f} us ({regime:5s}): "
                f"period {meas_period/1000:8.2f} us ({p_err:+.3f}%)  "
                f"width {meas_width/1000:7.2f} us (err {w_err_ns:+.0f} ns)  "
                f"{'PASS' if ok else 'FAIL'}"
            )
    finally:
        ts.set_slope(0, "POS")
    return all_ok


# ---- Test 3: long periods (GP regime) -------------------------------------

# Well into the GP regime. The ~0.03 Hz (34 s) floor is real but impractical to
# sample here; a few Hz exercises the same slow path in seconds.
SLOW_FREQS_HZ = [1_000, 100, 10]


def test_long_periods(ts, pg, duration_s):
    print("\n=== Long periods (GP regime rate) ===")
    pg.set_mode(Pulsegen.ASYNC)
    all_ok = True
    for f in SLOW_FREQS_HZ:
        period_ns = NS / f
        # Need >=50 edges; at the slowest rate stretch the window to suit.
        window = max(duration_s, 60.0 / f)
        pg.pulse(0, 1.0 / f, (1.0 / f) / 2)
        err = pg.get_error()
        if not err.startswith("0,"):
            raise RuntimeError(f"pulsegen error at {f} Hz: {err}")
        ts.discard_pending(settle_s=0.1)
        records = collect(ts, window, channels=(0,))
        seq = by_channel(records).get(0, [])
        iv = strict_interval(seq, tol_ns=80)
        if iv is None or len(seq) < 20:
            raise RuntimeError(f"too few samples at {f} Hz ({len(seq)})")
        err_pct = 100.0 * (iv - period_ns) / period_ns
        ok = abs(err_pct) <= 0.1
        all_ok = all_ok and ok
        print(
            f"  {f:5d} Hz (expect {period_ns/1000:8.1f} us): "
            f"{iv/1000:8.1f} us ({err_pct:+.3f}%)  ({len(seq)} edges)  "
            f"{'PASS' if ok else 'FAIL'}"
        )
    return all_ok


# ---- Test 4: cross-timer sync (stair) -------------------------------------

# A SYNC staircase: channel n's rising edge delayed n*step from ch0. Every
# channel must phase-lock to ch0 at exactly its step. Fast cases live on four
# distinct HRTIM timers (HRTIM master); the slow case is the GP regime (TIM1
# master, ITR0). Fine steps resolve only to +-1 timestamper tick.
STAIR_FAST_PERIOD_NS = 200_000  # 200 us -> 4 ns HRTIM tick, fits a 4 ns step
STAIR_FAST_STEPS_NS = [8, 100, 1_000]
STAIR_SLOW_PERIOD_NS = 2_000_000  # 2 ms -> GP regime
STAIR_SLOW_STEP_NS = 100_000  # 100 us


def step_tolerance_ns(step_ns):
    # +-1 timestamper tick for the sub-2-tick steps (coherent-clock limit),
    # tighter (proportional) for larger ones.
    return max(4.0, 2.0 + 0.01 * step_ns)


def measure_stair(ts, pg, period_ns, step_ns, duration_s):
    """SYNC stair at period_ns with ch n delayed n*step_ns. Return {ch: mean
    (ch - ch0) gap in ns} for ch 1..3."""
    width_ns = min(period_ns / 4, max(step_ns * 4, 1_000))
    pg.stair(period_ns / NS, width_ns / NS, step_ns / NS)
    err = pg.get_error()
    if not err.startswith("0,"):
        raise RuntimeError(f"pulsegen error in stair (step {step_ns} ns): {err}")
    ts.discard_pending(settle_s=0.2)
    records = collect(ts, duration_s)
    times = by_channel(records)
    ch0 = times.get(0, [])
    out = {}
    for ch in range(1, NUM_CHANNELS):
        deltas = zipper_pair(ch0, times.get(ch, []), period_ns)
        if len(deltas) < 50:
            raise RuntimeError(f"only {len(deltas)} ch0/ch{ch} pairs (need >=50)")
        out[ch] = statistics.fmean(deltas)
    return out


def test_sync_stair(ts, pg, duration_s):
    print("\n=== SYNC cross-timer stair (all four channels phase-lock to ch0) ===")
    all_ok = True

    cases = [(STAIR_FAST_PERIOD_NS, s, "HRTIM") for s in STAIR_FAST_STEPS_NS]
    cases.append((STAIR_SLOW_PERIOD_NS, STAIR_SLOW_STEP_NS, "GP"))
    for period_ns, step_ns, regime in cases:
        gaps = measure_stair(ts, pg, period_ns, step_ns, duration_s)
        tol = step_tolerance_ns(step_ns)
        line_ok = True
        parts = []
        for ch in range(1, NUM_CHANNELS):
            expect = ch * step_ns
            err = gaps[ch] - expect
            ch_ok = abs(err) <= tol
            line_ok = line_ok and ch_ok
            parts.append(f"ch{ch} {gaps[ch]:.1f} (err {err:+.1f})")
        all_ok = all_ok and line_ok
        print(
            f"  {regime:5s} step {step_ns:7d} ns (tol +-{tol:.1f}): "
            f"{'  '.join(parts)}  {'PASS' if line_ok else 'FAIL'}"
        )
    return all_ok


# ---- Test 5: burst in both regimes ----------------------------------------

# (label, spacing_s, width_s, ncyc, rep_s). Fast cases land in the HRTIM regime
# (BMC-gated); the slow case is above the ~524 us crossover (GP software count).
BURST_CASES = [
    ("fast/HRTIM", 10e-6, 2.5e-6, 50, 0.10),
    ("fast/HRTIM", 1e-6, 0.25e-6, 1000, 0.10),
    ("slow/GP", 1e-3, 0.25e-3, 5, 0.10),
]
BURST_SPACING_TOL_NS = 12

# Invalid burst parameters; each must latch a -200 error. ncyc max is 65534
# (BURST_NCYC_MAX = 65536 - BURST_IDLE_MIN); the interval must exceed one whole
# burst frame and stay <= 60 s.
BURST_ERROR_CASES = [
    ("SOUR0:BURS:NCYC 0", "-200"),
    ("SOUR0:BURS:NCYC 65535", "-200"),
    ("SOUR0:BURS:INT:PER 1e-4", "-200"),  # 100 us < one frame of 50x10 us
    ("SOUR0:BURS:INT:PER 61", "-200"),  # > 60 s ceiling
]


def burst_rep_tol_s(spacing_s):
    # The repetition is hardware-timed (TIM5), so it is exact apart from the
    # per-frame snap to a pulse-period boundary (<= one spacing). Tolerate two
    # spacings, floored at 2 ms -- far tighter than the old scheduler jiffy.
    return max(2e-3, 2 * spacing_s)


def measure_burst(ts, pg, spacing_s, width_s, ncyc, rep_s, duration_s):
    """Run one burst on ch0; return (sizes, worst_spacing_err_ns, rep_gaps_s)
    over interior bursts (the window truncates the first and last)."""
    pg.burst(0, spacing_s, width_s, ncyc, rep_s)
    err = pg.get_error()
    if not err.startswith("0,"):
        raise RuntimeError(f"pulsegen error configuring burst: {err}")
    ts.discard_pending(settle_s=0.3)
    records = collect(ts, duration_s, channels=(0,))
    times = sorted(t for _, t, _ in records)
    bursts = group_bursts(times, int(rep_s * NS / 2))
    if len(bursts) < 4:
        raise RuntimeError(f"only {len(bursts)} bursts captured (need >=4 for interior bursts)")
    interior = bursts[1:-1]
    sizes = [len(b) for b in interior]
    spacing_ns = spacing_s * NS
    worst_err = max(
        (abs((b - a) - spacing_ns) for burst in interior for a, b in zip(burst, burst[1:])),
        default=0,
    )
    rep_gaps = [(b[0] - a[0]) / NS for a, b in zip(interior, interior[1:])]
    return sizes, worst_err, rep_gaps


def test_burst(ts, pg, duration_s):
    print("\n=== Burst (exact N-cycle, both regimes) ===")
    duration_s = max(duration_s, 2.0)  # need >=4 bursts at the 0.1 s rep
    pg.set_mode(Pulsegen.ASYNC)  # single-channel burst; clear any SYNC left by an earlier test
    all_ok = True

    for label, spacing_s, width_s, ncyc, rep_s in BURST_CASES:
        sizes, worst_err, rep_gaps = measure_burst(
            ts, pg, spacing_s, width_s, ncyc, rep_s, duration_s
        )
        rep_tol = burst_rep_tol_s(spacing_s)
        count_ok = all(s == ncyc for s in sizes)
        spacing_ok = worst_err <= BURST_SPACING_TOL_NS
        rep_ok = all(abs(g - rep_s) <= rep_tol for g in rep_gaps)
        ok = count_ok and spacing_ok and rep_ok
        all_ok = all_ok and ok
        print(
            f"  {label:10s} ncyc {ncyc:5d} @ {spacing_s*1e6:.0f} us, rep {rep_s} s: "
            f"counts {min(sizes)}..{max(sizes)} ({len(sizes)} bursts)  "
            f"spacing err {worst_err:.0f} ns  "
            f"rep {min(rep_gaps):.4f}..{max(rep_gaps):.4f} s (tol +-{rep_tol*1e3:.0f} ms)  "
            f"{'PASS' if ok else 'FAIL'}"
        )

    # Error latching: each invalid parameter against a running burst.
    for cmd, want_code in BURST_ERROR_CASES:
        pg.burst(0, 10e-6, 2.5e-6, 50, 0.25)
        pg.send(cmd)
        err = pg.get_error()
        ok = err.startswith(want_code)
        all_ok = all_ok and ok
        print(f"  reject [{cmd}]: {err}  {'PASS' if ok else 'FAIL'}")

    # Disabling a live burst fails safe: the domain's outputs turn OFF rather
    # than streaming at the burst's instantaneous (intra-burst) rate.
    pg.pulse_burst(0, 10_000, 50)
    pg.set_burst_state(0, False)
    ts.discard_pending(settle_s=0.2)
    records = collect(ts, 1.0, channels=(0,))
    ok = len(records) == 0
    all_ok = all_ok and ok
    print(
        f"  burst off -> outputs off: {len(records)} pulses in 1 s (expect 0)  "
        f"{'PASS' if ok else 'FAIL'}"
    )

    # pulse() after a burst returns the channel to a continuous train.
    pg.pulse(0, 1e-5, 2.5e-6)
    ts.discard_pending(settle_s=0.2)
    records = collect(ts, 1.0, channels=(0,))
    expect_min = int(0.5 * NS / 10_000)  # half the window at 10 us periods
    ok = len(records) >= expect_min
    all_ok = all_ok and ok
    print(
        f"  pulse() after burst -> continuous: {len(records)} pulses in 1 s "
        f"(>= {expect_min})  {'PASS' if ok else 'FAIL'}"
    )
    return all_ok


# ---- Test 6: per-pin AF (every channel emits in both regimes) --------------

AF_FAST_HZ = 5_000  # all four together: 20 k/s, within budget at divider 1
AF_SLOW_HZ = 200  # GP regime


def test_per_pin_af(ts, pg, duration_s):
    print("\n=== Per-pin AF (all four channels emit, both regimes) ===")
    all_ok = True
    for label, hz in (("fast/HRTIM", AF_FAST_HZ), ("slow/GP", AF_SLOW_HZ)):
        pg.set_mode(Pulsegen.ASYNC)
        for ch in range(NUM_CHANNELS):
            pg.pulse(ch, 1.0 / hz, (1.0 / hz) / 2)
        err = pg.get_error()
        if not err.startswith("0,"):
            raise RuntimeError(f"pulsegen error in AF test ({label}): {err}")
        ts.discard_pending(settle_s=0.1)
        records = collect(ts, max(duration_s, 0.5))
        counts = {ch: 0 for ch in range(NUM_CHANNELS)}
        for ch, _, _ in records:
            counts[ch] += 1
        expect_min = int(0.3 * max(duration_s, 0.5) * hz)  # generous lower bound
        line_ok = all(counts[ch] >= expect_min for ch in range(NUM_CHANNELS))
        all_ok = all_ok and line_ok
        per_ch = "  ".join(f"ch{ch} {counts[ch]}" for ch in range(NUM_CHANNELS))
        print(
            f"  {label:10s} ({hz} Hz, expect >= {expect_min}/ch): {per_ch}  "
            f"{'PASS' if line_ok else 'FAIL'}"
        )
    return all_ok


# ---- Driver ---------------------------------------------------------------

TESTS = {
    "independent": test_async_independent,
    "handoff": test_handoff,
    "slow": test_long_periods,
    "sync": test_sync_stair,
    "burst": test_burst,
    "af": test_per_pin_af,
}


def main():
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--ts-port", default=None, help="Timestamper serial port (default: autodetect)")
    p.add_argument("--pg-port", default=None, help="Pulsegen serial port (default: autodetect)")
    p.add_argument(
        "--duration", type=float, default=2.0, help="Capture seconds per measurement (default: 2.0)"
    )
    p.add_argument("--only", choices=list(TESTS), default=None, help="Run only one test group")
    args = p.parse_args()

    selected = [args.only] if args.only else list(TESTS)
    results = {}
    with Pulsegen(port=args.pg_port) as pg, LectroTIC4(port=args.ts_port) as ts:
        print(f"Pulsegen:    {pg.idn()}")
        print(f"Timestamper: {ts.idn()}")
        try:
            pg.reset()
            configure_ts(ts)
            for name in selected:
                results[name] = TESTS[name](ts, pg, args.duration)
        finally:
            pg.off()
            pg.reset()
            ts.reset()

    print("\n=== Summary ===")
    for name, ok in results.items():
        print(f"  {name:14s} {'PASS' if ok else 'FAIL'}")
    all_ok = all(results.values())
    print(f"\n{'ALL PASS' if all_ok else 'FAILURES PRESENT'}")
    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
