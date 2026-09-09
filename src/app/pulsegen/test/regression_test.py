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
  7. SUB-TICK (250 ps) PLACEMENT. Sweep a SYNC delay in 250 ps steps at a
     CKPSC=0 period and watch the timestamper's 4 ns-quantized mean phase
     advance: physical jitter dithers edges across bin boundaries, so the mean
     interpolates below the tick. Working placement shows one 4 ns bin per
     sixteen 250 ps steps with crossings positioned by the analog base phase;
     a shifted second sweep must move the crossings accordingly (~4-5 min).

Caveats:
  - The two devices share the 10 MHz clock, so sub-2-tick gaps resolve only to
    +-1 timestamper tick (4 ns); wider gaps are tight (the subtick sweep gets
    below the tick only statistically, via jitter dither).

Run:
  regression_test.py
  regression_test.py --only sync --duration 3
  regression_test.py --only gp-startup
  regression_test.py --only gp-enable
  regression_test.py --only gp-wrap
  regression_test.py --only gp-burst-boundary
  regression_test.py --only rep-startup  # includes a 41-second capture
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
# Match the LT4 suite's allowance for independently delivered polarity batches.
EDGE_BATCH_SKEW_NS = 250_000_000


# ---- Measurement helpers --------------------------------------------------


def decode_records(records, channels=range(NUM_CHANNELS)):
    """Decode an already-armed timestamp stream into
    (channel, abs_ns, polarity) for the requested channels. abs_ns =
    seconds*1e9 + nanoseconds, kept as an exact int (the library's
    seconds/nanoseconds are ints, so no float ever touches a timestamp);
    polarity is '+' (rising) or '-' (falling).

    Raises immediately if the timestamper reports a lost pulse (overcapture or
    buffer overflow) or an oscillator failure -- a regression test must see
    every pulse, so any loss is a failure, not something to tolerate."""
    channels = set(channels)
    for rec in records:
        if isinstance(rec, Timestamp):
            if rec.channel in channels:
                yield (rec.channel, rec.seconds * NS + rec.nanoseconds, rec.polarity)
        elif isinstance(rec, PulsesLost):
            if rec.overcaptures or rec.buf_overflows:
                raise RuntimeError(
                    f"timestamper lost pulses on ch{rec.channel}: "
                    f"{rec.overcaptures} overcaptures, "
                    f"{rec.buf_overflows} buffer overflows"
                )
        elif isinstance(rec, OscillatorFailure):
            raise RuntimeError("timestamper reported oscillator failure")


def collect_started(ts, duration_s, start, channels=range(NUM_CHANNELS)):
    """Arm/framing-sync LT4 before starting the source, preserving its very first pulse.

    read_for() performs its marker sync eagerly, before returning the lazy record iterator. Do
    not call read_for(), discard_pending(), or any LT4 query again between start and consumption.
    """
    records = ts.read_for(duration_s)
    try:
        if start is not None:
            start()
        return list(decode_records(records, channels))
    finally:
        records.close()


def collect(ts, duration_s, channels=range(NUM_CHANNELS)):
    """Collect a normal marker-synchronized window after source configuration."""
    return collect_started(ts, duration_s, None, channels)


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


def zipper_pair(ref, other, period_ns, tol_ns=12):
    """Pair two sorted edge lists index-for-index: the i-th `other` edge pairs
    with the i-th `ref` edge (each period, `other` fires gap-after `ref`, with
    0 <= gap < one period). Return the list of (other - ref) gaps.

    The capture window does not begin or end on a clean period boundary, and
    the device delivers each channel in batches, so at each end one channel can
    lead the other by many edges. Those boundary edges have no partner -- window
    edges, not missed pulses -- so trim any orphan (an `other` whose `ref` is
    before the window, or a `ref` whose `other` is after it) from both ends.

    After trimming we are strict: equal counts, every pair within one period,
    and every same-channel spacing positive and within tol_ns of the requested
    period. The default allows three LT4 timestamp ticks of quantization/jitter.
    Any failing means an interior pulse was dropped, duplicated, or misplaced."""
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
    for label, seq in (("ref", ref), ("other", other)):
        for a, b in zip(seq, seq[1:]):
            gap = b - a
            if gap <= 0 or abs(gap - period_ns) > tol_ns:
                raise RuntimeError(
                    f"{label} channel gap {gap} ns differs from the requested "
                    f"{period_ns} ns period (tolerance {tol_ns} ns) -- "
                    "a pulse was dropped, duplicated, or misplaced"
                )
    return deltas


def pulse_widths(records, channel, allow_leading_fall=True, period_ns=None):
    """Per-pulse high time (ns) on `channel` from a both-edges capture: pair
    each rising edge with the next falling edge. Records must include both
    polarities (set the channel's slope to BOTH first). Every interior edge must alternate.
    With period_ns, restrict widths to the two polarity streams' common coverage: independent
    DMA batches can leave several boundary edges without their partners. Discarded coverage may
    not exceed one nominal period plus EDGE_BATCH_SKEW_NS. Set allow_leading_fall=False for an armed startup capture:
    its leading edge is never trimmed, and it must be rising. Rising-edge cadence is scored
    separately on the full, untrimmed capture.
    """
    edges = sorted((t, pol) for ch, t, pol in records if ch == channel)
    if any(b[0] <= a[0] for a, b in zip(edges, edges[1:])):
        raise RuntimeError(f"ch{channel}: duplicate or coincident edges")
    if any(pol not in ("+", "-") for _, pol in edges):
        raise RuntimeError(f"ch{channel}: invalid edge polarity")
    if period_ns is not None and edges:
        if period_ns <= 0:
            raise ValueError("pulse period must be positive")
        rises = [t for t, pol in edges if pol == "+"]
        falls = [t for t, pol in edges if pol == "-"]
        if not rises or not falls:
            raise RuntimeError(f"ch{channel}: missing rising or falling sub-stream")
        # A complete pulse can span at most one period. Keep the rise preceding the first
        # observed fall and the fall following the last observed rise, when present.
        lo = max(rises[0], falls[0] - period_ns) if allow_leading_fall else edges[0][0]
        hi = min(rises[-1] + period_ns, falls[-1])
        # The window can stop between two edges of a slow pulse, even without batching skew.
        coverage_allowance_ns = period_ns + EDGE_BATCH_SKEW_NS
        if lo - edges[0][0] > coverage_allowance_ns or edges[-1][0] - hi > coverage_allowance_ns:
            raise RuntimeError(
                f"ch{channel}: polarity coverage differs by more than one period + 250 ms"
            )
        if lo > hi:
            raise RuntimeError(f"ch{channel}: no common rising/falling coverage")
        edges = [(t, pol) for t, pol in edges if lo <= t <= hi]
    widths = []
    rising = None
    for index, (t, pol) in enumerate(edges):
        if pol == "+":
            if rising is not None:
                raise RuntimeError(f"ch{channel}: two rising edges without a falling edge")
            rising = t
        elif pol != "-":
            raise RuntimeError(f"ch{channel}: invalid edge polarity {pol!r}")
        elif rising is not None:
            if t <= rising:
                raise RuntimeError(f"ch{channel}: pulse width must be positive")
            widths.append(t - rising)
            rising = None
        elif index != 0 or not allow_leading_fall:
            raise RuntimeError(f"ch{channel}: falling edge without a preceding rising edge")
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
            widths = pulse_widths(records, 0, period_ns=period_ns)
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


# ---- GP startup and guard-offset regressions ------------------------------

GP_PULSE_TOL_NS = 12
GP_START_PERIODS_NS = (1_000_000, NS)
GP_WRAP_PERIOD_NS = 1_000_000
GP_WRAP_WIDTH_NS = 10_000
GP_WRAP_DELAYS_NS = (970_000, 974_000, 980_000, 984_000, 990_000)


def check_pg_error(pg, context):
    err = pg.get_error()
    if not err.startswith("0,"):
        raise RuntimeError(f"pulsegen error {context}: {err}")


def stage_gp(pg, channels, period_ns, width_ns, delay_ns=0):
    """Leave all outputs OFF, with the selected channels ready for one final enable command."""
    pg.off()
    for ch in range(NUM_CHANNELS):
        pg.set_burst_state(ch, False)
    for ch in channels:
        pg.set_period(ch, period_ns / NS)
        pg.set_width(ch, width_ns / NS)
        pg.set_delay(ch, delay_ns / NS)


def prime_gp_prescalers(ts, pg, channels, period_ns):
    """Observe two stable natural periods per timer before testing its next configuration.

    *RST does not reset timer registers. The first old-prescaler cycle may last nearly 34 seconds
    if an earlier test used the maximum prescaler, so bound the wait generously but return as soon
    as the observed periods establish the requested prescaler. Priming, unlike the measurement,
    deliberately ignores initial transients.
    """
    channels = tuple(channels)
    pg.off()
    pg.set_mode(Pulsegen.ASYNC)
    stage_gp(pg, channels, period_ns, period_ns // 4)
    for ch in channels:
        ts.set_slope(ch, "POS")
        ts.set_divider(ch, 1)
        pg.set_state(ch, True)
    check_pg_error(pg, "priming GP timers")

    recent = {ch: [] for ch in channels}
    ready = set()
    records = ts.read_for(35.0 + 3 * period_ns / NS)
    try:
        for ch, timestamp, polarity in decode_records(records, channels):
            if polarity != "+":
                raise RuntimeError(f"unexpected falling edge while priming ch{ch}")
            recent[ch].append(timestamp)
            recent[ch] = recent[ch][-3:]
            if len(recent[ch]) == 3 and all(
                abs((b - a) - period_ns) <= GP_PULSE_TOL_NS
                for a, b in zip(recent[ch], recent[ch][1:])
            ):
                ready.add(ch)
            else:
                ready.discard(ch)
            if ready == set(channels):
                return
    finally:
        records.close()
    raise RuntimeError(
        f"GP timers never reached their priming period: {sorted(set(channels) - ready)}"
    )


def check_gp_pulses(records, ch, period_ns, width_ns, startup=False):
    """Check every observed period/width, including the first, without window trimming.

    There is no timestamped hardware enable marker, so this cannot prove that an entire first
    pulse was absent. Startup cases deliberately choose a wide pulse that remains observable
    under the known stale-prescaler failure; USB wall-clock counts are not a substitute for that.
    """
    rising = by_channel(records).get(ch, [])
    widths = pulse_widths(records, ch, allow_leading_fall=not startup, period_ns=period_ns)
    minimum = 3 if startup else 20
    if len(rising) < minimum or len(widths) < minimum - 1:
        raise RuntimeError(
            f"too few complete pulses on ch{ch} ({len(rising)} rises, {len(widths)} widths)"
        )
    gaps = [b - a for a, b in zip(rising, rising[1:])]
    bad_gaps = sum(abs(gap - period_ns) > GP_PULSE_TOL_NS for gap in gaps)
    bad_widths = sum(abs(width - width_ns) > GP_PULSE_TOL_NS for width in widths)
    ok = not (bad_gaps or bad_widths)
    print(
        f"  ch{ch}: first gap {gaps[0]:.0f} ns, first width {widths[0]:.0f} ns; "
        f"wrong gaps {bad_gaps}/{len(gaps)}, widths {bad_widths}/{len(widths)}  "
        f"{'PASS' if ok else 'FAIL'}"
    )
    if not ok:
        print(f"    first gaps (ns): {gaps[:4]}; first widths (ns): {widths[:4]}")
    return ok


def run_gp_startup_case(ts, pg, ch, seed_ns, target_ns, width_ns, duration_s):
    """Prime one timer, stage it while off, then score its first enabled pulses."""
    prime_gp_prescalers(ts, pg, (ch,), seed_ns)
    stage_gp(pg, (ch,), target_ns, width_ns)
    ts.set_slope(ch, "BOTH")
    window = max(duration_s, 3 * max(seed_ns, target_ns) / NS + 0.3)
    records = collect_started(ts, window, lambda: pg.set_state(ch, True), channels=(ch,))
    check_pg_error(pg, "starting GP output")
    print(f"  ch{ch}: {seed_ns / NS:g} s -> {target_ns / NS:g} s, width {width_ns / NS:g} s")
    return check_gp_pulses(records, ch, target_ns, width_ns, startup=True)


def test_gp_startup(ts, pg, duration_s):
    print("\n=== GP first period and width after prescaler changes ===")
    all_ok = True
    for ch in range(NUM_CHANNELS):
        for seed_ns, target_ns in (GP_START_PERIODS_NS, GP_START_PERIODS_NS[::-1]):
            # Both directions remain observable even with the stale prescaler: the nominal
            # 250 ms pulse becomes 250 us at the old fast rate, rather than disappearing below
            # LT4's capture bandwidth. The reverse direction can stretch a cycle to one second.
            ok = run_gp_startup_case(ts, pg, ch, seed_ns, target_ns, target_ns // 4, duration_s)
            all_ok = ok and all_ok
    return all_ok


def test_gp_enable(ts, pg, duration_s):
    print("\n=== GP output enable with unchanged prescaler ===")
    all_ok = True
    for ch in range(NUM_CHANNELS):
        # Keeping the latched prescaler unchanged isolates output-enable ordering. A wide pulse
        # exposes an output enabled while CNT still holds its pre-reset position.
        ok = run_gp_startup_case(ts, pg, ch, NS, NS, 3 * NS // 4, duration_s)
        all_ok = ok and all_ok
    return all_ok


def test_gp_wrap(ts, pg, duration_s):
    print("\n=== GP width across the guard-shifted period boundary ===")
    channels = tuple(range(NUM_CHANNELS))
    prime_gp_prescalers(ts, pg, channels, GP_WRAP_PERIOD_NS)
    for ch in channels:
        ts.set_slope(ch, "BOTH")
        ts.set_divider(ch, 1)
    all_ok = True
    try:
        for mode in (Pulsegen.ASYNC, Pulsegen.SYNC):
            pg.off()
            pg.set_mode(mode)
            for delay_ns in GP_WRAP_DELAYS_NS:
                stage_gp(pg, channels, GP_WRAP_PERIOD_NS, GP_WRAP_WIDTH_NS, delay_ns)
                for ch in channels:
                    pg.set_state(ch, True)
                check_pg_error(pg, f"configuring guard-boundary delay {delay_ns} ns")
                # This measures steady-state wrapping, not startup. The GP prescalers above are
                # already latched; allow the multi-command output setup to finish before capture.
                ts.discard_pending(settle_s=0.1)
                records = collect(ts, max(duration_s, 0.1))
                print(f"  {mode.value}: delay {delay_ns} ns, width {GP_WRAP_WIDTH_NS} ns")
                for ch in channels:
                    ok = check_gp_pulses(records, ch, GP_WRAP_PERIOD_NS, GP_WRAP_WIDTH_NS)
                    all_ok = ok and all_ok
    finally:
        for ch in channels:
            ts.set_slope(ch, "POS")
    return all_ok


def measure_gp_sync_offsets(ts, pg, width_ns, duration_s):
    """Measure a separate flat continuous control, never calibrate from the tested burst.

    TIM1 drives ch3 directly; ch0/1/2 follow its TRGO through slave reset controllers. Their fixed
    trigger-path delay, plus cable/input skew, is not a burst framing error. Check every control
    period/width and phase sample before using the signed mean offsets for burst comparisons.
    """
    channels = tuple(range(NUM_CHANNELS))
    pg.off()
    pg.set_mode(Pulsegen.SYNC)
    stage_gp(pg, channels, GP_WRAP_PERIOD_NS, width_ns)
    try:
        for ch in channels:
            pg.set_state(ch, True)
        check_pg_error(pg, "configuring flat GP SYNC control")
        ts.discard_pending(settle_s=0.1)
        records = collect(ts, min(max(duration_s, 0.3), 1.0))
        rising = by_channel(records)
        offsets = {}
        for ch in channels:
            if not check_gp_pulses(records, ch, GP_WRAP_PERIOD_NS, width_ns):
                raise RuntimeError(f"invalid GP SYNC control waveform on ch{ch}")
            phases = [
                (timestamp - rising[0][0] + GP_WRAP_PERIOD_NS // 2) % GP_WRAP_PERIOD_NS
                - GP_WRAP_PERIOD_NS // 2
                for timestamp in rising[ch]
            ]
            offset = statistics.mean(phases)
            if any(abs(phase - offset) > GP_PULSE_TOL_NS for phase in phases):
                raise RuntimeError(f"unstable GP SYNC control phase on ch{ch}")
            offsets[ch] = offset
        print(f"  flat continuous SYNC offsets (ns): {offsets}")
        return offsets
    finally:
        pg.off()


def check_gp_burst_frame(
    records, delays_ns, width_ns, ncyc, period_ns, sync=False, phase_offsets_ns=None
):
    """Score one complete, armed frame, retaining every first/last edge and pulse."""
    rising = by_channel(records)
    falling = by_channel(records, "-")
    reference_ch = min(delays_ns)
    reference = rising.get(reference_ch, [])
    offsets = phase_offsets_ns if phase_offsets_ns is not None else dict.fromkeys(delays_ns, 0)
    all_ok = True
    for ch, delay_ns in delays_ns.items():
        rises, falls = rising.get(ch, []), falling.get(ch, [])
        try:
            # No coverage clipping: the finite frame ended well before the collection deadline.
            widths = pulse_widths(records, ch, allow_leading_fall=False)
        except RuntimeError as exc:
            print(f"  ch{ch}: {exc}  FAIL")
            all_ok = False
            continue
        gaps = [b - a for a, b in zip(rises, rises[1:])]
        ok = len(rises) == len(falls) == len(widths) == ncyc
        ok = ok and all(abs(width - width_ns) <= GP_PULSE_TOL_NS for width in widths)
        ok = ok and all(abs(gap - period_ns) <= GP_PULSE_TOL_NS for gap in gaps)
        phase_errors = []
        if sync:
            phase_errors = [
                (other - ref)
                - (delay_ns - delays_ns[reference_ch])
                - (offsets[ch] - offsets[reference_ch])
                for ref, other in zip(reference, rises)
            ]
            ok = ok and len(reference) == ncyc
            ok = ok and all(abs(error) <= GP_PULSE_TOL_NS for error in phase_errors)
        print(
            f"  ch{ch}: {len(rises)} rises/{len(falls)} falls, expect {ncyc}; "
            f"widths {widths[:4]}, gaps {gaps[:4]}, phase errors {phase_errors[:4]} ns  "
            f"{'PASS' if ok else 'FAIL'}"
        )
        all_ok = ok and all_ok
    return all_ok


def run_gp_burst_boundary_case(
    ts, pg, mode, delays_ns, width_ns, ncyc, duration_s, phase_offsets_ns=None
):
    """Stage a finite first frame, then start it with exactly one post-marker PG command."""
    channels = tuple(delays_ns)
    control_ch = channels[0]
    pg.off()
    pg.set_mode(mode)
    stage_gp(pg, channels, GP_WRAP_PERIOD_NS, width_ns)
    for ch, delay_ns in delays_ns.items():
        pg.set_delay(ch, delay_ns / NS)
    pg.set_burst_ncycles(control_ch, ncyc)
    pg.set_burst_period(control_ch, 60.0)
    pg.set_burst_state(control_ch, True)

    if mode == Pulsegen.SYNC:
        # Enabling channels is not atomic and each setter restarts all enabled outputs. Let all
        # configuration frames finish and reach LT4 before its framing clear; the unchanged count
        # setter then restarts all four outputs together after the capture is armed.
        for ch in channels:
            pg.set_state(ch, True)
        time.sleep(0.3)
        start = lambda: pg.set_burst_ncycles(control_ch, ncyc)
    else:
        start = lambda: pg.set_state(control_ch, True)
    check_pg_error(pg, "staging GP boundary burst")
    # A longer window cannot add first-frame coverage, and must not reach the next repeat.
    window = min(max(duration_s, 0.3), 1.0)
    try:
        records = collect_started(ts, window, start, channels=channels)
        check_pg_error(pg, "starting GP boundary burst")
        print(f"  {mode.value}: {ncyc} pulses, delays {delays_ns}, width {width_ns} ns")
        return check_gp_burst_frame(
            records,
            delays_ns,
            width_ns,
            ncyc,
            GP_WRAP_PERIOD_NS,
            sync=mode == Pulsegen.SYNC,
            phase_offsets_ns=phase_offsets_ns,
        )
    finally:
        pg.off()


def test_gp_burst_boundary(ts, pg, duration_s):
    print("\n=== GP complete first bursts at both period boundaries ===")
    channels = tuple(range(NUM_CHANNELS))
    prime_gp_prescalers(ts, pg, channels, GP_WRAP_PERIOD_NS)
    pg.off()
    for ch in channels:
        ts.set_slope(ch, "BOTH")
        ts.set_divider(ch, 1)
    all_ok = True
    try:
        phase_offsets = measure_gp_sync_offsets(ts, pg, 750_000, duration_s)
        for ncyc in (1, 3):
            for ch in channels:
                for delay_ns in (0, GP_WRAP_PERIOD_NS - GP_WRAP_WIDTH_NS):
                    ok = run_gp_burst_boundary_case(
                        ts, pg, Pulsegen.ASYNC, {ch: delay_ns}, GP_WRAP_WIDTH_NS, ncyc, duration_s
                    )
                    all_ok = ok and all_ok
            # These high windows collectively cover the entire period, so a software gating
            # scheme cannot rely on finding a shared idle gap in which to toggle the outputs.
            ok = run_gp_burst_boundary_case(
                ts,
                pg,
                Pulsegen.SYNC,
                dict(enumerate((0, 100_000, 200_000, 250_000))),
                750_000,
                ncyc,
                duration_s,
                phase_offsets_ns=phase_offsets,
            )
            all_ok = ok and all_ok
    finally:
        pg.off()
        for ch in channels:
            ts.set_slope(ch, "POS")
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


REP_START_SPACING_NS = 100_000
REP_START_WIDTH_NS = 25_000
REP_START_NCYC = 3


def stage_repetition(pg, rep_s):
    pg.off()
    pg.set_mode(Pulsegen.ASYNC)
    for ch in range(NUM_CHANNELS):
        pg.set_burst_state(ch, False)
    pg.set_period(0, REP_START_SPACING_NS / NS)
    pg.set_width(0, REP_START_WIDTH_NS / NS)
    pg.set_delay(0, 0)
    pg.set_burst_ncycles(0, REP_START_NCYC)
    pg.set_burst_period(0, rep_s)
    pg.set_burst_state(0, True)


def repetition_metrics(records):
    if any(ch != 0 or polarity != "+" for ch, _, polarity in records):
        raise RuntimeError("unexpected channel or polarity in repetition capture")
    times = by_channel(records).get(0, [])
    # Use the within-frame cadence to split groups. rep/2 would merge the exact half-length
    # initial interval that the unlatched TIM5 prescaler regression needs to expose.
    bursts = group_bursts(times, 4 * REP_START_SPACING_NS)
    if len(bursts) < 2:
        raise RuntimeError(f"only {len(bursts)} complete burst starts; need at least two")
    sizes = [len(burst) for burst in bursts]
    spacing_errors = [
        abs((b - a) - REP_START_SPACING_NS) for burst in bursts for a, b in zip(burst, burst[1:])
    ]
    gaps = [b[0] - a[0] for a, b in zip(bursts, bursts[1:])]
    return sizes, max(spacing_errors, default=0), gaps


def check_repetition_startup(records, rep_s):
    sizes, worst_spacing, gaps = repetition_metrics(records)
    tol_ns = round(burst_rep_tol_s(REP_START_SPACING_NS / NS) * NS)
    bad = sum(abs(gap - round(rep_s * NS)) > tol_ns for gap in gaps)
    ok = (
        all(size == REP_START_NCYC for size in sizes)
        and worst_spacing <= BURST_SPACING_TOL_NS
        and not bad
    )
    print(
        f"  repetition {rep_s:g} s: first interval {gaps[0] / NS:.6f} s; "
        f"counts {sizes}, spacing error {worst_spacing} ns, "
        f"wrong intervals {bad}/{len(gaps)} (tol {tol_ns / 1e6:g} ms)  "
        f"{'PASS' if ok else 'FAIL'}"
    )
    return ok


def test_rep_startup(ts, pg, duration_s):
    print("\n=== TIM5 first repetition after prescaler changes (includes 41 s capture) ===")
    ts.set_slope(0, "POS")
    ts.set_divider(0, 1)
    pg.off()
    pg.set_mode(Pulsegen.ASYNC)
    # Establish div=1 from observed natural intervals, independent of TIM5's old register state.
    # Its only other supported divisor is 2, so the priming first interval can be at most 0.2 s.
    sizes, spacing, gaps = measure_burst(
        ts, pg, REP_START_SPACING_NS / NS, REP_START_WIDTH_NS / NS, REP_START_NCYC, 0.1, 1.0
    )
    if (
        not all(size == REP_START_NCYC for size in sizes)
        or spacing > BURST_SPACING_TOL_NS
        or any(abs(gap - 0.1) > burst_rep_tol_s(REP_START_SPACING_NS / NS) for gap in gaps)
    ):
        raise RuntimeError("TIM5 never reached its 0.1 s priming cadence")

    all_ok = True
    # The first overflow in the 40 s capture latches div=2, even on the broken firmware where it
    # arrives at 20 s. That observed overflow then primes the reverse 0.1 s transition below.
    for rep_s, minimum_window in ((40.0, 41.0), (0.1, 1.0)):
        stage_repetition(pg, rep_s)
        records = collect_started(
            ts, max(duration_s, minimum_window), lambda: pg.set_state(0, True)
        )
        check_pg_error(pg, "starting repetition timer")
        ok = check_repetition_startup(records, rep_s)
        all_ok = ok and all_ok
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


# ---- Test 7: sub-tick (250 ps) delay placement ----------------------------

# SYNC at a CKPSC=0 period (250 ps grid), ch0 fixed and ch1 swept in 250 ps
# steps. Each channel's phase is timestamp mod period -- exact, because both
# instruments share the reference -- and although the timestamper quantizes to
# 4 ns, physical jitter dithers edges across bin boundaries so the mean over
# thousands of pulses interpolates below the tick. Working 250 ps placement
# shows one 4 ns bin per SIXTEEN steps, with crossing positions set by the
# analog base phase; sweep B moves the common base +1 ns and every crossing
# must move ~4 steps earlier (mod 16). A delay path internally quantized to
# 4 ns can do neither.
SUBTICK_PERIOD_NS = 10_000  # CKPSC=0 (<= ~16.4 us)
SUBTICK_WIDTH_NS = 1_000
SUBTICK_BASE_PS = 1_000_000
SUBTICK_STEP_PS = 250
SUBTICK_STEPS = 64  # 16 ns span = 4 bin transitions
SUBTICK_DIVIDER = 32  # keeps 2 x 100 kHz within the record budget
SUBTICK_CAPTURE_S = 1.0


def subtick_phase(times):
    """Circular-safe mean of (t mod period), centered on the median."""
    mods = [t % SUBTICK_PERIOD_NS for t in times]
    m = statistics.median(mods)
    half = SUBTICK_PERIOD_NS // 2
    return m + statistics.fmean(((x - m + half) % SUBTICK_PERIOD_NS) - half for x in mods)


def subtick_measure(ts):
    ts.discard_pending(settle_s=0.2)
    times = by_channel(collect(ts, SUBTICK_CAPTURE_S, channels=(0, 1)))
    if len(times.get(0, [])) < 500 or len(times.get(1, [])) < 500:
        raise RuntimeError(
            f"subtick: too few samples ({len(times.get(0, []))}/{len(times.get(1, []))})"
        )
    d = (subtick_phase(times[1]) - subtick_phase(times[0])) % SUBTICK_PERIOD_NS
    return d - SUBTICK_PERIOD_NS if d > SUBTICK_PERIOD_NS / 2 else d


def subtick_sweep(ts, pg, base_ps, label):
    """Set both channels' base, wait for the relative phase to settle (a full
    reconfig is followed by a slow ns-scale drift), then sweep ch1."""
    pg.set_delay(0, base_ps / 1e12)
    pg.set_delay(1, base_ps / 1e12)
    err = pg.get_error()
    if not err.startswith("0,"):
        raise RuntimeError(f"subtick base config: {err}")
    prev = subtick_measure(ts)
    for i in range(20):
        cur = subtick_measure(ts)
        if abs(cur - prev) < 0.15:
            break
        prev = cur
    print(f"  {label}: settled after {i + 1} checks (drift now {abs(cur - prev):.3f} ns)")

    deltas = []
    for k in range(SUBTICK_STEPS):
        pg.set_delay(1, (base_ps + k * SUBTICK_STEP_PS) / 1e12)
        err = pg.get_error()
        if not err.startswith("0,"):
            raise RuntimeError(f"subtick k={k}: {err}")
        deltas.append(subtick_measure(ts))
    return deltas


def subtick_centers(deltas):
    """First k at which the mean rises past each bin midpoint (2, 6, 10, 14 ns
    above the sweep's starting plateau)."""
    base = deltas[0]
    centers = []
    for level in (2.0, 6.0, 10.0, 14.0):
        for k, d in enumerate(deltas):
            if d - base >= level:
                centers.append(k)
                break
    return centers


def test_subtick(ts, pg, duration_s):
    print("\n=== Sub-tick (250 ps) delay placement ===")
    pg.set_mode(Pulsegen.SYNC)
    for c in range(NUM_CHANNELS):
        pg.set_period(c, SUBTICK_PERIOD_NS / NS)
        pg.set_width(c, SUBTICK_WIDTH_NS / NS)
        pg.set_delay(c, SUBTICK_BASE_PS / 1e12)
        pg.set_state(c, c in (0, 1))
    err = pg.get_error()
    if not err.startswith("0,"):
        raise RuntimeError(f"subtick setup: {err}")
    for c in (0, 1):
        ts.set_divider(c, SUBTICK_DIVIDER)

    a = subtick_sweep(ts, pg, SUBTICK_BASE_PS, "sweep A")
    b = subtick_sweep(ts, pg, SUBTICK_BASE_PS + 1_000, "sweep B (+1 ns base)")

    all_ok = True
    ks = range(SUBTICK_STEPS)
    mean_k = statistics.fmean(ks)
    denom = sum((k - mean_k) ** 2 for k in ks)
    for name, d in (("A", a), ("B", b)):
        mean_d = statistics.fmean(d)
        slope = sum((k - mean_k) * (x - mean_d) for k, x in zip(ks, d)) / denom
        slope /= SUBTICK_STEP_PS / 1000  # ns of motion per ns commanded
        ok = 0.85 <= slope <= 1.15
        all_ok = all_ok and ok
        print(
            f"  sweep {name}: slope {slope:.3f} (1.0 = each 250 ps step moves the "
            f"edge 250 ps)  {'PASS' if ok else 'FAIL'}"
        )

    ca, cb = subtick_centers(a), subtick_centers(b)
    for name, c in (("A", ca), ("B", cb)):
        spacings = [j - i for i, j in zip(c, c[1:])]
        ok = len(c) >= 3 and all(14 <= s <= 18 for s in spacings)
        all_ok = all_ok and ok
        print(
            f"  sweep {name}: bin transitions at k={c} (spacings {spacings}; "
            f"expect 16 +-2)  {'PASS' if ok else 'FAIL'}"
        )

    # +1 ns base = +4 steps: crossings arrive ~4 steps earlier, modulo the
    # 16-step bin. Compare per-sweep crossing phase within the bin cycle.
    if ca and cb:
        shifts = [(((j - i) + 8) % 16) - 8 for i, j in zip(ca, cb)]
        med = statistics.median(shifts)
        ok = -6.0 <= med <= -2.0
        all_ok = all_ok and ok
        print(
            f"  base shift moved crossings {shifts} steps mod 16 (median {med:+.1f}; "
            f"expect ~-4)  {'PASS' if ok else 'FAIL'}"
        )
    else:
        all_ok = False
        print("  FAIL: too few crossings to compare sweeps")
    return all_ok


# ---- Driver ---------------------------------------------------------------

TESTS = {
    "independent": test_async_independent,
    "handoff": test_handoff,
    "slow": test_long_periods,
    "gp-startup": test_gp_startup,
    "gp-enable": test_gp_enable,
    "gp-wrap": test_gp_wrap,
    "gp-burst-boundary": test_gp_burst_boundary,
    "sync": test_sync_stair,
    "burst": test_burst,
    "rep-startup": test_rep_startup,
    "af": test_per_pin_af,
    "subtick": test_subtick,
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
            for name in selected:
                # Every test starts from silence and default capture config, so
                # no test's leftover state (e.g. channels still emitting) can
                # poison the next one's capture.
                pg.reset()
                configure_ts(ts)
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
