#!/usr/bin/env python3

"""LectroTIC-4 + Rigol DG1022Z end-to-end regression test.

Exercises every wire/host path of the 4-channel device:

  * TEXT mode  -- the device's ASCII stream, read straight off the
    serial port and parsed by this file's own reader (also the only
    way to see the device's '#' overcapture/overflow diagnostics,
    which the firmware suppresses in binary mode).
  * BINARY mode -- the 8-byte wire format, decoded by the tsctl
    library (LectroTIC4.read_for); this also exercises the
    marker-synced binary framing.
  * tsctl CLI  -- the command-line tool itself, driven as a
    subprocess once the library has released the port.

Every capturing phase runs in BOTH text and binary; the streaming-rate
expectations scale with the mode (a binary record is 8 bytes, a text
record ~22, so binary sustains a much higher edge rate).

Phases:
  rising / both / divider / output-gating / burst / sustained-rate
      -- run in text AND binary
  scpi-errors / reset / single-pulse / idn-serial   -- run once
  tsctl-cli   -- the CLI tool end to end

The signal generator's output must be wired to the input selected
with --channel (default 0).

Usage:
  regression_test.py
  regression_test.py --channel 1
  regression_test.py --duration 10
  regression_test.py --siggen-host siggen2
"""

import argparse
import os
import re
import statistics
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from tsctl import LectroTIC4
from siggen import Siggen

TSCTL = os.path.join(os.path.dirname(__file__), "tsctl.py")

PULSE_HZ = 10
PULSE_WIDTH_S = 0.001  # siggen.periodic() hardcodes 1 ms
PERIOD_S = 1.0 / PULSE_HZ

# Per-gap timing tolerance, applied to EVERY device-timestamp interval
# in every phase. The siggen and the device both run off the same
# 10 MHz lab standard, so there is no frequency drift between them; the
# device timestamps with 4 ns resolution. A clean gap is therefore
# nominal ± a handful of ns (siggen DDS quantization + one capture
# tick). A single missed pulse, by contrast, moves a gap by a whole
# period -- ≥40 µs at 25 kHz, 100 ms at 10 Hz -- thousands of times
# outside this bound. One tolerance, one checker, every phase: any
# interval off nominal by more than this is a dropped/extra edge or a
# real timing fault, never measurement noise.
GAP_TOL_S = 12e-9

# Sustained continuous-rate target per mode: the device must keep up
# with this edge rate for SUSTAINED_S seconds with no loss. Text is
# ~22 bytes/record vs binary's 8, so its USB-drain ceiling is far
# lower -- hence the per-mode targets.
SUSTAINED_HZ = {"text": 25000, "binary": 100000}
SUSTAINED_S = 5.0

# Firmware status line: "# ch<N>: <X> overcaptures, <Y> buf overflows"
_DIAG_RE = re.compile(
    r"#\s*ch(\d+):\s*(\d+)\s+overcaptures,\s*(\d+)\s+buf overflows")


def expect(condition, message):
    print(f"  [{'PASS' if condition else 'FAIL'}] {message}")
    return bool(condition)


# --------------------------------------------------------------------
# Capture modes
#
# Both return the uniform tuple
#   (times, others, overcaptures, overflows, comments)
# overcaptures/overflows are ints in text mode and None in binary mode
# (the firmware suppresses the '#' diagnostic lines on the binary
# wire -- there, loss is caught by the per-phase record-count check).
# --------------------------------------------------------------------

def parse_text_stream(raw, channel):
    """Parse the device's raw TEXT output (bytes)."""
    times, others, comments = [], set(), []
    overcaptures = overflows = 0
    for line in raw.decode(errors="replace").splitlines():
        line = line.strip()
        if not line:
            continue
        if line.startswith("#"):
            comments.append(line)
            m = _DIAG_RE.search(line)
            if m:
                overcaptures += int(m.group(2))
                overflows += int(m.group(3))
            continue
        parts = line.split()
        if len(parts) < 2:
            continue
        try:
            ch = int(parts[0])
            sec_str, _, ns_str = parts[1].partition(".")
            t = int(sec_str) + (int(ns_str) if ns_str else 0) * 1e-9
        except ValueError:
            continue
        if ch != channel:
            others.add(ch)
            continue
        times.append(t)
    return times, others, overcaptures, overflows, comments


class TextMode:
    name = "text"

    def capture(self, tic, channel, duration_s):
        tic.send("FORM:DATA TEXT")
        tic.set_stream_enabled(True)
        tic.discard_pending()  # marker-synced clean start
        return parse_text_stream(tic.read_raw(duration_s), channel)

    def arrivals(self, tic, channel, window_s, want):
        """Host monotonic arrival time of each record on `channel`,
        for up to window_s, stopping once `want` are seen."""
        tic.send("FORM:DATA TEXT")
        tic.set_stream_enabled(True)
        tic.discard_pending()
        out, raw, seen = [], b"", 0
        start = time.monotonic()
        while time.monotonic() - start < window_s and len(out) < want:
            chunk = tic.read_raw(0.05)
            if not chunk:
                continue
            raw += chunk
            ts, *_ = parse_text_stream(raw, channel)
            while seen < len(ts):
                out.append(time.monotonic())
                seen += 1
        return out


class BinaryMode:
    name = "binary"

    def capture(self, tic, channel, duration_s):
        # read_for() does FORM:DATA BIN + the marker-sync framing +
        # stream-enable, then yields decoded Records for the window.
        times, others = [], set()
        for r in tic.read_for(duration_s):
            if r.channel != channel:
                others.add(r.channel)
                continue
            times.append(r.seconds + r.nanoseconds * 1e-9)
        return times, others, None, None, []

    def arrivals(self, tic, channel, window_s, want):
        """Host monotonic arrival time of each record on `channel`
        (read_for yields as bytes decode), stopping once `want` seen."""
        out = []
        deadline = time.monotonic() + window_s
        for r in tic.read_for(window_s):
            if r.channel == channel:
                out.append(time.monotonic())
                if len(out) >= want:
                    break
            if time.monotonic() >= deadline:
                break
        return out


MODES = [TextMode(), BinaryMode()]


def expect_no_loss(mode, ovc, ovf):
    """In text mode the device reports overcapture/overflow on the
    wire; assert zero. In binary those '#' lines are suppressed, so
    loss is caught by the caller's record-count assertion -- note it."""
    if mode.name == "text":
        ok = expect(ovc == 0, "no overcaptures")
        ok &= expect(ovf == 0, "no ring-buffer overflows")
        return ok
    print("  [INFO] overcapture/overflow not on the binary wire; "
          "loss is covered by the record-count check")
    return True


def intervals_within(deltas, target, tol, label):
    """All deltas within tol of target. On failure, print every
    violator (index, value, deviation) so a boundary artifact vs a
    real timing fault is immediately distinguishable. Deviations are
    shown in ns: a clean gap is a few ns off, a missed pulse is a
    whole period off, so the magnitude alone tells which it is."""
    bad = [(i, d) for i, d in enumerate(deltas) if abs(d - target) >= tol]
    if bad:
        shown = bad[:6]
        print(f"    [DIAG] {label}: {len(bad)}/{len(deltas)} off "
              f"target {target * 1e3:.3f} ms by >{tol * 1e9:.0f} ns: " +
              ", ".join(f"#{i}={d * 1e3:.4f}ms"
                        f"(Δ{(d - target) * 1e9:+.0f}ns)"
                        for i, d in shown) +
              (" …" if len(bad) > len(shown) else ""))
    return not bad


def report_intervals(label, times):
    if len(times) < 2:
        print(f"  {label}: <2 samples, can't compute intervals")
        return None
    deltas = [(times[i + 1] - times[i]) for i in range(len(times) - 1)]
    print(f"  {label}: count={len(deltas)} "
          f"min={min(deltas) * 1e3:.3f} ms "
          f"median={statistics.median(deltas) * 1e3:.3f} ms "
          f"max={max(deltas) * 1e3:.3f} ms "
          f"stdev={statistics.stdev(deltas) * 1e6:.1f} µs")
    return deltas


# --------------------------------------------------------------------
# Shared scoring/classification helpers
# --------------------------------------------------------------------

def collected(times, others, channel, expected):
    print(f"  collected {len(times)} on ch{channel} "
          f"(expected ~{expected:.1f}); others={sorted(others) or 'none'}")


def score_clean(mode, ovc, ovf, others):
    """The checks every capturing phase shares: no overcapture /
    ring overflow (visible on the text wire), and no records leaking
    in from a channel other than the one under test."""
    ok = expect_no_loss(mode, ovc, ovf)
    ok &= expect(not others, "no spurious other-channel activity")
    return ok


def classify_gaps(deltas, split):
    """Split gaps into short (< split) / long (>= split). 1 ms vs
    99 ms are 98 ms apart so the split is unambiguous. Returns
    (classes, shorts, longs)."""
    cls = ["S" if d < split else "L" for d in deltas]
    short = [d for d, c in zip(deltas, cls) if c == "S"]
    long = [d for d, c in zip(deltas, cls) if c == "L"]
    return cls, short, long


# --------------------------------------------------------------------
# Mode-parameterized capturing phases
# --------------------------------------------------------------------

def phase_periodic(mode, sg, tic, channel, duration_s, divider,
                   count_tol):
    """Rising-edge-only periodic capture. divider==1 is the plain
    rising phase; divider>1 is the divider phase (1 stamp per N
    pulses). One body, parameterized. Establishes its own siggen
    signal so phase order / prior state can't poison it."""
    label = ("rising edge only" if divider == 1
             else f"rising, divider={divider}")
    print(f"\n=== {label} [{mode.name}] ===")
    sg.periodic(PULSE_HZ)
    tic.set_slope(channel, "POS")
    tic.set_divider(channel, divider)
    times, others, ovc, ovf, _ = mode.capture(tic, channel, duration_s)
    expected = (PULSE_HZ / divider) * duration_s
    period = PERIOD_S * divider
    collected(times, others, channel, expected)
    deltas = report_intervals("intervals", times)

    ok = expect(abs(len(times) - expected) <= count_tol,
                f"count within {count_tol} of expected {expected:.1f}")
    ok &= score_clean(mode, ovc, ovf, others)
    if deltas:
        ok &= expect(intervals_within(deltas, period, GAP_TOL_S,
                                      "intervals"),
                     f"every interval within {GAP_TOL_S * 1e9:.0f} ns "
                     f"of {period * 1e3:.0f} ms (no pulse missed)")
    return ok


def phase_both(mode, sg, tic, channel, duration_s):
    print(f"\n=== both edges [{mode.name}] ===")
    sg.periodic(PULSE_HZ)
    tic.set_slope(channel, "BOTH")
    tic.set_divider(channel, 1)
    times, others, ovc, ovf, _ = mode.capture(tic, channel, duration_s)
    expected = 2 * PULSE_HZ * duration_s
    collected(times, others, channel, expected)
    if len(times) < 4:
        return expect(False, "enough samples for interval analysis")

    # BOTH mode captures both edges of each pulse, so the gaps must
    # strictly alternate: short (~pulse width) then long (~period -
    # width), forever. The PERIOD_S/2 split only sorts each gap into
    # its class (1 ms vs 99 ms are 98 ms apart -- unambiguous); once
    # sorted, every gap in each class is held to the same GAP_TOL_S as
    # every other phase, so a single missed edge anywhere is caught.
    deltas = [times[i + 1] - times[i] for i in range(len(times) - 1)]
    cls, short, long = classify_gaps(deltas, PERIOD_S / 2)
    long_target = PERIOD_S - PULSE_WIDTH_S
    # The capture window opens/closes asynchronously to the pulse
    # train, so the first and last gap can straddle a pulse whose
    # other edge lay outside the window -- a half-clipped pulse reads
    # as one full period, not period-minus-width. Per-gap exactness is
    # therefore asserted on the interior; strict alternation and the
    # count check below span the whole sequence and catch any dropped
    # interior edge (one missing edge puts two same-class gaps
    # adjacent, breaking alternation).
    _, ishort, ilong = classify_gaps(deltas[1:-1], PERIOD_S / 2)
    print(f"  short(rise→fall) n={len(short)} "
          f"med={statistics.median(short) * 1e3:.3f} ms; "
          f"long(fall→rise) n={len(long)} "
          f"med={statistics.median(long) * 1e3:.3f} ms")

    ok = expect(abs(len(times) - expected) <= 4,
                f"count within 4 of expected {expected:.0f}")
    ok &= score_clean(mode, ovc, ovf, others)
    ok &= expect(abs(len(short) - len(long)) <= 1,
                 "short/long delta counts roughly equal")
    ok &= expect(len(cls) >= 4 and
                 all(cls[i] != cls[i + 1] for i in range(len(cls) - 1)),
                 "gaps strictly alternate short/long (every pulse = "
                 "2 edges, regardless of start)")
    ok &= expect(bool(ishort) and
                 intervals_within(ishort, PULSE_WIDTH_S, GAP_TOL_S,
                                  "short(rise→fall)"),
                 f"every interior short gap within "
                 f"{GAP_TOL_S * 1e9:.0f} ns of "
                 f"{PULSE_WIDTH_S * 1e3:.0f} ms")
    ok &= expect(bool(ilong) and
                 intervals_within(ilong, long_target, GAP_TOL_S,
                                  "long(fall→rise)"),
                 f"every interior long gap within "
                 f"{GAP_TOL_S * 1e9:.0f} ns of "
                 f"{long_target * 1e3:.0f} ms")
    return ok


def phase_output_gating(mode, sg, tic, channel):
    print(f"\n=== OUTPut:STATe gating [{mode.name}] ===")
    sg.periodic(PULSE_HZ)
    tic.set_slope(channel, "POS")
    tic.set_divider(channel, 1)

    # Stream OFF: not one byte may arrive.
    tic.set_stream_enabled(False)
    tic.reset_input_buffer()
    bytes_off = tic.read_raw(1.0)

    # Stream ON: a fresh window must produce ~PULSE_HZ stamps, decoded
    # through this mode's path.
    times, others, ovc, ovf, _ = mode.capture(tic, channel, 1.0)
    print(f"  OFF: {len(bytes_off)} bytes (expect 0); "
          f"ON: {len(times)} stamps (expect ~{PULSE_HZ})")

    ok = expect(len(bytes_off) == 0, "silent while OUTP:STAT OFF")
    ok &= expect(abs(len(times) - PULSE_HZ) <= 2,
                 f"~{PULSE_HZ} stamps after OUTP:STAT ON")
    return ok


def _group_bursts(times):
    """Split times into bursts: records within 0.5 s are one burst
    (the siggen repeats a burst every 1 s). Returns burst sizes."""
    sizes, i = [], 0
    while i < len(times):
        j = i + 1
        while j < len(times) and abs(times[j] - times[i]) < 0.5:
            j += 1
        sizes.append(j - i)
        i = j
    return sizes


def capture_burst(mode, sg, tic, channel, slope, ncyc, spacing_ns,
                  window_s=3.5):
    tic.set_slope(channel, slope)
    tic.set_divider(channel, 1)
    actual_ns = sg.pulse_burst(spacing_ns, ncyc=ncyc)
    times, others, ovc, ovf, comments = mode.capture(
        tic, channel, window_s)
    for c in comments:
        print(f"    | {c}")
    return actual_ns, _group_bursts(times), others, ovc, ovf


def phase_burst(mode, sg, tic, channel, ncyc=16383, spacing_ns=100):
    print(f"\n=== {ncyc}-pulse rising burst @{spacing_ns} ns "
          f"[{mode.name}] ===")
    actual_ns, sizes, others, ovc, ovf = capture_burst(
        mode, sg, tic, channel, "POS", ncyc, spacing_ns)
    # Only the first and last burst can be clipped by the capture
    # window; every burst that begins AND ends inside the window must
    # have captured all ncyc pulses. A single dropped pulse anywhere
    # in any interior burst (16382 instead of 16383) fails this.
    interior = sizes[1:-1]
    short = [(idx + 1, n) for idx, n in enumerate(interior)
             if n != ncyc]
    print(f"  spacing {actual_ns:.1f} ns; bursts {sizes}; "
          f"interior must all == {ncyc}; "
          f"short interior (idx,n)={short or 'none'}; "
          f"others={sorted(others) or 'none'}")

    ok = expect(len(interior) >= 1,
                f">=1 fully-captured (interior) burst of {ncyc}")
    ok &= expect(not short,
                 f"every interior burst is exactly {ncyc} pulses "
                 f"(not one missed)")
    ok &= score_clean(mode, ovc, ovf, others)
    return ok


def phase_sustained(mode, sg, tic, channel):
    hz = SUSTAINED_HZ[mode.name]
    print(f"\n=== sustained {hz} Hz for {SUSTAINED_S:.0f}s "
          f"[{mode.name}] ===")
    tic.set_slope(channel, "POS")
    tic.set_divider(channel, 1)
    # Continuous (non-burst) pulse train at the mode's rated rate.
    sg.periodic(hz, pulse_width_s=min(1e-6, 0.2 / hz))
    # Transition isolation: a preceding burst phase can leave the
    # device's DMA capture buffer holding high-rate residue that the
    # firmware drains into the (just-cleared) ring after the marker
    # sync, contaminating a rate measurement. Drain through that
    # residue with the new signal already running, so the measured
    # window is pure steady-rate.
    tic.discard_pending()
    tic.read_raw(0.5)
    times, others, ovc, ovf, _ = mode.capture(tic, channel, SUSTAINED_S)
    # Rate is measured from the captured records' own time span, not
    # count/window: the latter loses ~(marker-sync + end-of-window
    # drain) at both ends regardless of rate. A device that keeps up
    # streams at the true source rate with no gaps; one that can't
    # either drops chunks (binary: a span-rate deficit and a gap) or
    # overflows (text: the '#' line, asserted via expect_no_loss).
    n = len(times)
    span = (times[-1] - times[0]) if n >= 2 else 0
    obs_hz = (n - 1) / span if span > 0 else 0
    deltas = [times[i + 1] - times[i] for i in range(n - 1)]
    nominal = 1.0 / hz
    print(f"  collected {n} on ch{channel}; observed "
          f"{obs_hz:.0f} Hz over {span:.3f}s "
          f"(nominal gap {nominal * 1e6:.2f} µs); "
          f"others={sorted(others) or 'none'}")

    ok = expect(n >= 100, f"sustained {hz} Hz produced enough records")
    # Every inter-record gap must be the source period to within
    # GAP_TOL_S -- the same bound every other phase uses. At this rate
    # the device cannot keep up by streaming a smooth-but-wrong rate:
    # falling behind drops chunks, and a dropped edge is a >=1*nominal
    # gap excursion (>=40 µs at 25 kHz), thousands of ns past tol.
    if deltas:
        ok &= expect(intervals_within(deltas, nominal, GAP_TOL_S,
                                      "sustained gap"),
                     f"every gap within {GAP_TOL_S * 1e9:.0f} ns of "
                     f"the {nominal * 1e6:.2f} µs source period "
                     f"(not a single pulse missed)")
    ok &= score_clean(mode, ovc, ovf, others)
    return ok


# --------------------------------------------------------------------
# Mode-independent phases (run once)
# --------------------------------------------------------------------

def phase_scpi_errors(tic):
    print("\n=== SCPI error handling ===")
    # Clean text slate: a preceding binary phase can leave undrained
    # binary bytes that readline() would mis-read as the first error
    # response. The marker sync flushes both sides.
    tic.send("FORM:DATA TEXT")
    tic.set_stream_enabled(False)
    tic.discard_pending()
    tic.clear_errors()
    ok = expect(tic.get_error().startswith("0,"),
                "SYST:ERR? after *CLS reports no-error")
    for cmd, code in [("INP0:DIV 0", "-222"),
                      ("INP0:SLOP FROBNICATE", "-104"),
                      ("FOOBAR", "-100"),
                      ("INP0:DIV notanumber", "-104"),
                      ("INP4:SLOP POS", "-100")]:  # ch 4 out of 0..3
        tic.clear_errors()
        tic.reset_input_buffer()
        tic.send(cmd)
        err = tic.get_error()
        ok &= expect(code in err, f"`{cmd}` -> {code} (got {err!r})")
    ok &= expect(tic.get_error().startswith("0,"),
                 "SYST:ERR? auto-clears after read")
    ok &= expect(tic.idn().startswith("Lectrobox,LectroTIC-4"),
                 "device still answers *IDN?")
    return ok


def phase_reset(tic, channel):
    print("\n=== *RST ===")
    for ch in range(4):
        tic.set_slope(ch, "BOTH")
        tic.set_divider(ch, 7)
    tic.set_stream_enabled(False)
    tic.reset_input_buffer()
    ok = expect(tic.get_slope(channel) == "BOTH", "pre-RST slope BOTH")
    ok &= expect(tic.get_divider(channel) == "7", "pre-RST divider 7")
    ok &= expect(tic.get_stream_enabled() is False, "pre-RST stream off")

    tic.reset()
    tic.reset_input_buffer()
    for ch in range(4):
        ok &= expect(tic.get_slope(ch) == "POS", f"ch{ch} slope -> POS")
        ok &= expect(tic.get_divider(ch) == "1", f"ch{ch} divider -> 1")
    ok &= expect(tic.get_stream_enabled() is True, "post-RST stream on")
    return ok


def phase_slope_switch(sg, tic, channel):
    """Switch POS -> NEG mid-capture and confirm the device starts
    detecting the opposite edge -- coverage the steady POS/BOTH phases
    don't give. Drive 10 Hz with a 10 ms-wide pulse (so the crossover
    gap is plainly != the 100 ms period). Capturing rising edges gives
    ~100 ms gaps; after SLOP NEG it captures falling edges, also
    ~100 ms apart; between them sits exactly ONE crossover gap of
    width + k*period (k = whole periods until the command takes
    effect; the host can't phase-lock that, so k is ~0/1/2 -- the
    value, not its determinism, is what we report). Mode-independent
    device behavior, so it runs once (text)."""
    WIDTH = 0.010
    print("\n=== slope switch POS->NEG mid-capture ===")
    sg.periodic(PULSE_HZ, pulse_width_s=WIDTH)
    tic.set_slope(channel, "POS")
    tic.set_divider(channel, 1)
    tic.send("FORM:DATA TEXT")
    tic.set_stream_enabled(True)
    tic.discard_pending()

    times, raw, seen, switched = [], b"", 0, False
    start = time.monotonic()
    while time.monotonic() - start < 6.0 and len(times) < 30:
        chunk = tic.read_raw(0.05)
        if chunk:
            raw += chunk
            ts, *_ = parse_text_stream(raw, channel)
            while seen < len(ts):
                times.append(ts[seen])
                seen += 1
        if not switched and len(times) >= 10:
            tic.send(f"INP{channel}:SLOP NEG")  # POS -> NEG mid-stream
            switched = True
    sg.output_off()

    # The 5 ms split only separates the lone crossover gap from the
    # steady run -- it's not the timing check. Once separated, the
    # steady gaps (rising-rising before, falling-falling after) are
    # held to GAP_TOL_S of the period like every other phase, and the
    # crossover to GAP_TOL_S of WIDTH + k*PERIOD_S; only k (whole
    # periods until the host's command lands) is non-deterministic.
    deltas = [times[i + 1] - times[i] for i in range(len(times) - 1)]
    steady_idx = [i for i, d in enumerate(deltas)
                  if abs(d - PERIOD_S) < 5e-3]
    anomalies = [(i, d) for i, d in enumerate(deltas)
                 if i not in steady_idx]
    steady_gaps = [deltas[i] for i in steady_idx]
    print(f"  collected {len(times)} edges, {len(deltas)} gaps; "
          f"anomalies (idx, ms): "
          f"{[(i, round(d * 1e3, 1)) for i, d in anomalies]}")

    ok = expect(switched, "INP:SLOP NEG sent mid-capture")
    ok &= expect(len(times) >= 20, ">=20 edges across the switch")
    ok &= expect(len(anomalies) == 1,
                 "exactly one (clean) transition gap -- a string of "
                 "~100 ms, one crossover, ~100 ms again")
    ok &= expect(bool(steady_gaps) and
                 intervals_within(steady_gaps, PERIOD_S, GAP_TOL_S,
                                  "steady (POS then NEG)"),
                 f"every steady gap within {GAP_TOL_S * 1e9:.0f} ns of "
                 f"{PERIOD_S * 1e3:.0f} ms, both sides of the switch "
                 f"(no edge missed)")
    if anomalies:
        i, d = anomalies[0]
        k = round((d - WIDTH) / PERIOD_S)
        ok &= expect(0 <= k <= 3 and
                     intervals_within([d], WIDTH + k * PERIOD_S,
                                      GAP_TOL_S, "crossover"),
                     f"transition gap {d * 1e3:.1f} ms = "
                     f"{WIDTH * 1e3:.0f} ms + {k}*{PERIOD_S * 1e3:.0f} "
                     f"ms (rising->falling crossover)")
        ok &= expect(i >= 5 and i <= len(deltas) - 5,
                     "healthy ~100 ms runs before AND after the "
                     "switch (NEG detection working)")
    return ok


def phase_single_pulse(mode, sg, tic, channel, n=5):
    """A lone, isolated pulse must reach the host promptly -- not be
    held back waiting for more activity. Drives one 50 ns pulse per
    second (siggen burst, ncyc=1, internal 1 s trigger) and times each
    record's host arrival via this mode's path (so the binary
    marker-synced framing is also exercised on the isolated-pulse
    case). With prompt flushing each pulse arrives in its own ~1 s
    slot; a stall would show as missing pulses or clumped arrivals."""
    print(f"\n=== single isolated pulse: timely delivery "
          f"[{mode.name}] ===")
    tic.set_slope(channel, "POS")
    tic.set_divider(channel, 1)
    sg.pulse_burst(1000, ncyc=1)  # one pulse, repeating every 1.000 s
    arrivals = mode.arrivals(tic, channel, n + 2.5, n + 1)
    sg.output_off()

    deltas = [arrivals[i + 1] - arrivals[i]
              for i in range(len(arrivals) - 1)]
    print(f"  received {len(arrivals)} isolated pulses; host "
          f"inter-arrival: {[f'{d:.2f}' for d in deltas]}")
    ok = expect(abs(len(arrivals) - n) <= 1,
                f"~{n} isolated pulses received (none starved)")
    if deltas:
        ok &= expect(max(deltas) < 1.3,
                     f"each pulse delivered in its own ~1 s slot "
                     f"(max gap {max(deltas):.2f}s < 1.3s)")
        ok &= expect(min(deltas) > 0.7,
                     f"no pulses batched together "
                     f"(min gap {min(deltas):.2f}s > 0.7s)")
    return ok


def phase_idn_serial(tic):
    print("\n=== *IDN? serial matches USB descriptor ===")
    tic.set_stream_enabled(False)
    tic.discard_pending()
    idn = tic.idn()
    parts = idn.split(",")
    idn_serial = parts[2] if len(parts) == 4 else None
    usb_serial = tic.usb_serial
    print(f"  {idn!r}; usb={usb_serial!r}")
    ok = expect(len(parts) == 4, "*IDN? has 4 fields")
    ok &= expect(bool(usb_serial), "USB descriptor serial readable")
    ok &= expect(idn_serial == usb_serial,
                 f"*IDN? serial == USB serial ({idn_serial!r})")
    return ok


# --------------------------------------------------------------------
# tsctl CLI (subprocess; runs with the library's port handle released)
# --------------------------------------------------------------------

def _cli(port, *args, timeout=15):
    return subprocess.run(
        [sys.executable, TSCTL, "--port", port, *args],
        capture_output=True, text=True, timeout=timeout)


def phase_tsctl_cli(port, sg):
    print("\n=== tsctl CLI ===")
    ok = True

    r = _cli(port, "idn")
    ok &= expect(r.returncode == 0 and
                 r.stdout.startswith("Lectrobox,LectroTIC-4"),
                 f"`tsctl idn` -> IDN ({r.stdout.strip()!r})")

    ok &= expect(_cli(port, "reset").returncode == 0, "`tsctl reset` ok")

    _cli(port, "slope", "0", "NEG")
    r = _cli(port, "slope", "0")
    ok &= expect(r.stdout.strip() == "NEG",
                 f"`tsctl slope 0` -> NEG ({r.stdout.strip()!r})")
    _cli(port, "slope", "0", "POS")

    _cli(port, "div", "0", "3")
    r = _cli(port, "div", "0")
    ok &= expect(r.stdout.strip() == "3",
                 f"`tsctl div 0` -> 3 ({r.stdout.strip()!r})")
    _cli(port, "div", "0", "1")

    _cli(port, "format", "BIN")
    r = _cli(port, "format")
    ok &= expect(r.stdout.strip().upper().startswith("BIN"),
                 f"`tsctl format` -> BIN ({r.stdout.strip()!r})")
    _cli(port, "format", "TEXT")

    r = _cli(port, "raw", "*IDN?")
    ok &= expect("LectroTIC-4" in r.stdout,
                 f"`tsctl raw *IDN?` -> IDN ({r.stdout.strip()!r})")

    # stream: a real signal, run the CLI briefly, parse its stdout.
    # -u so the child's stdout isn't lost in a pipe buffer on kill.
    sg.periodic(PULSE_HZ)
    _cli(port, "slope", "0", "POS")
    proc = subprocess.Popen(
        [sys.executable, "-u", TSCTL, "--port", port, "stream"],
        stdout=subprocess.PIPE, text=True)
    time.sleep(2.5)
    proc.terminate()
    try:
        out, _ = proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, _ = proc.communicate()
    sg.output_off()
    lines = [ln for ln in out.splitlines() if ln.strip()]
    line_re = re.compile(r"^\d+ \d+\.\d{9}$")
    good = [ln for ln in lines if line_re.match(ln)]
    print(f"  `tsctl stream`: {len(lines)} lines, {len(good)} well-formed")
    ok &= expect(len(good) >= 5 and len(good) == len(lines),
                 "all stream lines are '<ch> <sec>.<ns>'")
    ok &= expect(all(ln.split()[0] == "0" for ln in good),
                 "all stream records are on the driven channel 0")
    return ok


# --------------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default=None,
                   help="LectroTIC-4 serial port (default: autodetect)")
    p.add_argument("--siggen-host", default="siggen",
                   help="Signal generator hostname (default: siggen)")
    p.add_argument("--channel", type=int, choices=[0, 1, 2, 3],
                   default=0, help="Input channel under test")
    p.add_argument("--duration", type=float, default=5.0,
                   help="Capture window per steady phase, s (default 5)")
    args = p.parse_args()

    sg = Siggen(host=args.siggen_host)
    print(f"Signal generator: {sg.idn()}")
    results = []

    with LectroTIC4(args.port) as tic:
        port = tic.port
        print(f"LectroTIC-4: {port}; channel {args.channel}")
        print(f"Connected: {tic.idn()}")

        ch, dur = args.channel, args.duration
        # Each entry is (label, callable(mode)). Run for both modes.
        per_mode = [
            ("rising",
             lambda m: phase_periodic(m, sg, tic, ch, dur, 1, 2)),
            ("both", lambda m: phase_both(m, sg, tic, ch, dur)),
            ("divider",
             lambda m: phase_periodic(m, sg, tic, ch, dur, 5, 1)),
            ("output gating",
             lambda m: phase_output_gating(m, sg, tic, ch)),
            ("burst", lambda m: phase_burst(m, sg, tic, ch)),
            ("sustained", lambda m: phase_sustained(m, sg, tic, ch)),
            ("single pulse",
             lambda m: phase_single_pulse(m, sg, tic, ch)),
        ]
        once = [
            ("scpi errors", lambda: phase_scpi_errors(tic)),
            ("reset", lambda: phase_reset(tic, ch)),
            ("slope switch", lambda: phase_slope_switch(sg, tic, ch)),
            ("idn serial", lambda: phase_idn_serial(tic)),
        ]
        try:
            for mode in MODES:
                for label, fn in per_mode:
                    results.append(
                        (f"{label} [{mode.name}]", fn(mode)))
            for label, fn in once:
                results.append((label, fn()))
        finally:
            tic.set_stream_enabled(False)
            sg.output_off()

    # Port released -- now exercise the CLI tool itself.
    try:
        results.append(("tsctl cli", phase_tsctl_cli(port, sg)))
    finally:
        sg.output_off()
        sg.close()

    print("\n=== Summary ===")
    all_ok = True
    for name, ok in results:
        print(f"  {'PASS' if ok else 'FAIL'}: {name}")
        all_ok &= ok
    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
