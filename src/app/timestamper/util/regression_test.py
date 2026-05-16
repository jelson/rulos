#!/usr/bin/env python3

"""LectroTIC-4 + Rigol DG1022Z end-to-end regression test.

Exercises every wire/host path:

  * TEXT mode  -- the device's ASCII stream, read straight off the
    serial port and parsed by this file's own reader (also the only
    way to see the device's '#' overcapture/overflow diagnostics,
    which the firmware suppresses in binary mode).
  * BINARY mode -- the 8-byte wire format, decoded by the tsctl
    library (LectroTIC4.read_for), which is also how the marker-synced
    binary framing is tested.
  * tsctl CLI  -- the command-line tool itself, driven as a
    subprocess once the library has released the port.

Every capturing phase runs in BOTH text and binary; the streaming-rate
expectations scale with the mode (a binary record is 8 bytes, a text
record ~26, so binary sustains a much higher edge rate).

Polarity is hardware-latched (each input has a permanently rising- and
a permanently falling-armed TIM2 channel; both report as the input's
channel with an exact polarity bit, time-merged on the device). SLOPe
selects which are streamed.

Phases:
  rising / both / divider / output-gating / burst / polarity-burst /
  sustained-rate   -- run in text AND binary
  scpi-errors / reset / idn-serial   -- mode-independent, run once
  tsctl-cli   -- the CLI tool end to end

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
TOLERANCE_S = 1e-6

# Sustained continuous-rate target per mode: the device must keep up
# with this edge rate for a few seconds with no loss. Text is ~26
# bytes/record vs binary's 8, so its USB-drain ceiling is far lower;
# these are conservative (regression coverage of the sustained path,
# not ceiling-finding -- deadtime.py / sustained_rate.py do that).
SUSTAINED_HZ = {"text": 12000, "binary": 35000}
SUSTAINED_S = 2.0

# The polarity-burst phase caught a rare (~once per few bursts)
# seconds-rollover merge-order race; repeat it every run so a regression
# can't slip through on a lucky single shot.
POLARITY_BURST_REPEATS = 10

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
#   (times, edges, others, overcaptures, overflows, comments)
# where overcaptures/overflows are integers in text mode and None in
# binary mode (the firmware suppresses the '#' diagnostic lines on the
# binary wire -- there, loss is detected by the per-phase record-count
# check instead).
# --------------------------------------------------------------------

def parse_text_stream(raw, channel):
    """Parse the device's raw TEXT output (bytes)."""
    times, edges, others, comments = [], [], set(), []
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
        edges.append(parts[2] if len(parts) >= 3 else None)
    return times, edges, others, overcaptures, overflows, comments


class TextMode:
    name = "text"

    def capture(self, tic, channel, duration_s):
        tic.send("FORM:DATA TEXT")
        tic.set_stream_enabled(True)
        tic.discard_pending()  # marker-synced clean start
        return parse_text_stream(tic.read_raw(duration_s), channel)


class BinaryMode:
    name = "binary"

    def capture(self, tic, channel, duration_s):
        # read_for() does FORM:DATA BIN + the marker-sync framing +
        # stream-enable, then yields decoded Records for the window.
        times, edges, others = [], [], set()
        for r in tic.read_for(duration_s):
            if r.channel != channel:
                others.add(r.channel)
                continue
            times.append(r.seconds + r.nanoseconds * 1e-9)
            edges.append(r.edge)
        return times, edges, others, None, None, []


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
# Mode-parameterized capturing phases
# --------------------------------------------------------------------

def phase_rising(mode, tic, channel, duration_s):
    print(f"\n=== rising edge only [{mode.name}] ===")
    tic.set_slope(channel, "POS")
    tic.set_divider(channel, 1)
    times, edges, others, ovc, ovf, _ = mode.capture(
        tic, channel, duration_s)
    expected = PULSE_HZ * duration_s
    print(f"  collected {len(times)} on ch{channel} "
          f"(expected ~{expected:.0f}); others={sorted(others) or 'none'}")
    deltas = report_intervals("intervals", times)

    ok = expect(abs(len(times) - expected) <= 2,
                f"count within 2 of expected {expected:.0f}")
    ok &= expect_no_loss(mode, ovc, ovf)
    ok &= expect(bool(times) and all(e == "+" for e in edges),
                 "every edge is rising '+'")
    if deltas:
        ok &= expect(all(abs(d - PERIOD_S) < TOLERANCE_S for d in deltas),
                     f"all intervals within {TOLERANCE_S:.0e}s of "
                     f"{PERIOD_S * 1e3:.0f} ms")
    return ok


def phase_both(mode, tic, channel, duration_s):
    print(f"\n=== both edges [{mode.name}] ===")
    tic.set_slope(channel, "BOTH")
    tic.set_divider(channel, 1)
    times, edges, others, ovc, ovf, _ = mode.capture(
        tic, channel, duration_s)
    expected = 2 * PULSE_HZ * duration_s
    print(f"  collected {len(times)} on ch{channel} "
          f"(expected ~{expected:.0f}); others={sorted(others) or 'none'}")
    if len(times) < 4:
        return expect(False, "enough samples for interval analysis")

    deltas = [times[i + 1] - times[i] for i in range(len(times) - 1)]
    short = [d for d in deltas if d < PERIOD_S / 2]
    long = [d for d in deltas if d >= PERIOD_S / 2]
    print(f"  short(rise→fall) n={len(short)} "
          f"med={statistics.median(short) * 1e3:.3f} ms; "
          f"long(fall→rise) n={len(long)} "
          f"med={statistics.median(long) * 1e3:.3f} ms")

    ok = expect(abs(len(times) - expected) <= 4,
                f"count within 4 of expected {expected:.0f}")
    ok &= expect_no_loss(mode, ovc, ovf)
    ok &= expect(abs(len(short) - len(long)) <= 1,
                 "short/long delta counts roughly equal")
    if short:
        ok &= expect(all(abs(d - PULSE_WIDTH_S) < TOLERANCE_S
                         for d in short),
                     f"short intervals ~{PULSE_WIDTH_S * 1e3:.0f} ms")
    if long:
        ok &= expect(all(abs(d - (PERIOD_S - PULSE_WIDTH_S)) < TOLERANCE_S
                         for d in long),
                     f"long intervals ~"
                     f"{(PERIOD_S - PULSE_WIDTH_S) * 1e3:.0f} ms")
    ok &= expect(all(e in "+-" for e in edges) and
                 all(edges[i] != edges[i + 1]
                     for i in range(len(edges) - 1)),
                 "polarity strictly alternates +/-")
    ok &= expect(all((edges[i] == "+") == (deltas[i] < PERIOD_S / 2)
                     for i in range(len(deltas))),
                 "rising precedes the short gap, falling the long gap")
    return ok


def phase_divider(mode, tic, channel, duration_s, divider=5):
    print(f"\n=== rising, divider={divider} [{mode.name}] ===")
    tic.set_slope(channel, "POS")
    tic.set_divider(channel, divider)
    times, edges, others, ovc, ovf, _ = mode.capture(
        tic, channel, duration_s)
    expected = (PULSE_HZ / divider) * duration_s
    expected_period = PERIOD_S * divider
    print(f"  collected {len(times)} on ch{channel} "
          f"(expected ~{expected:.1f}); others={sorted(others) or 'none'}")
    deltas = report_intervals("intervals", times)

    ok = expect(abs(len(times) - expected) <= 1,
                f"count within 1 of expected {expected:.1f}")
    ok &= expect_no_loss(mode, ovc, ovf)
    ok &= expect(bool(times) and all(e == "+" for e in edges),
                 "every edge is rising '+'")
    if deltas:
        ok &= expect(all(abs(d - expected_period) < TOLERANCE_S
                         for d in deltas),
                     f"all intervals ~{expected_period * 1e3:.0f} ms")
    tic.set_divider(channel, 1)
    return ok


def phase_output_gating(mode, tic, channel):
    print(f"\n=== OUTPut:STATe gating [{mode.name}] ===")
    tic.set_slope(channel, "POS")
    tic.set_divider(channel, 1)

    # Stream OFF: not one byte may arrive.
    tic.set_stream_enabled(False)
    tic.reset_input_buffer()
    bytes_off = tic.read_raw(1.0)

    # Stream ON: a fresh window must produce ~PULSE_HZ stamps, decoded
    # through this mode's path.
    times, edges, others, ovc, ovf, _ = mode.capture(tic, channel, 1.0)
    print(f"  OFF: {len(bytes_off)} bytes (expect 0); "
          f"ON: {len(times)} stamps (expect ~{PULSE_HZ})")

    ok = expect(len(bytes_off) == 0, "silent while OUTP:STAT OFF")
    ok &= expect(abs(len(times) - PULSE_HZ) <= 2,
                 f"~{PULSE_HZ} stamps after OUTP:STAT ON")
    return ok


def _group_bursts(times, edges):
    bursts, i = [], 0
    while i < len(times):
        j = i + 1
        while j < len(times) and abs(times[j] - times[i]) < 0.5:
            j += 1
        bursts.append((j - i, edges[i:j]))
        i = j
    return bursts


def capture_burst(mode, sg, tic, channel, slope, ncyc, spacing_ns,
                  window_s=3.5):
    tic.set_slope(channel, slope)
    tic.set_divider(channel, 1)
    actual_ns = sg.pulse_burst(spacing_ns, ncyc=ncyc)
    times, edges, others, ovc, ovf, comments = mode.capture(
        tic, channel, window_s)
    for c in comments:
        print(f"    | {c}")
    return actual_ns, _group_bursts(times, edges), others, ovc, ovf


def phase_burst(mode, sg, tic, channel, ncyc=16383, spacing_ns=100):
    print(f"\n=== {ncyc}-pulse rising burst @{spacing_ns} ns "
          f"[{mode.name}] ===")
    actual_ns, bursts, others, ovc, ovf = capture_burst(
        mode, sg, tic, channel, "POS", ncyc, spacing_ns)
    sizes = [n for n, _ in bursts]
    complete = [n for n in sizes if n == ncyc]
    print(f"  spacing {actual_ns:.1f} ns; bursts {sizes}; "
          f"others={sorted(others) or 'none'}")

    ok = expect(len(complete) >= 2,
                f"at least 2 complete bursts of {ncyc} pulses")
    ok &= expect_no_loss(mode, ovc, ovf)
    ok &= expect(not others, "no spurious other-channel activity")
    return ok


def _burst_alt_ok(e):
    return (e and e[0] == "+" and all(c in "+-" for c in e) and
            all(e[i] != e[i + 1] for i in range(len(e) - 1)))


def _polarity_burst_once(mode, sg, tic, channel, ncyc, spacing_ns,
                         expected):
    """One max-rate BOTH burst capture + checks. Returns
    (ok, summary, diag) -- diag is a string only on alternation
    failure (so the rare merge-order race is pinpointed)."""
    actual_ns, bursts, others, ovc, ovf = capture_burst(
        mode, sg, tic, channel, "BOTH", ncyc, spacing_ns, window_s=2.0)
    full = [(n, e) for n, e in bursts if n == expected]
    alt_ok = bool(full) and all(_burst_alt_ok(e) for _, e in full)

    diag = None
    if full and not alt_ok:
        for e in (e for _, e in full if not _burst_alt_ok(e)):
            brk = next((i for i in range(len(e) - 1)
                        if e[i] == e[i + 1]), None)
            diag = (f"first16={''.join(e[:16])} e[0]={e[0]!r} " +
                    (f"first repeat at {brk}: "
                     f"…{''.join(e[max(0, brk - 4):brk + 5])}…"
                     if brk is not None else
                     "(no adjacent repeat; e[0] != '+')"))
            break

    loss_ok = (ovc == 0 and ovf == 0) if mode.name == "text" else True
    ok = len(full) >= 2 and alt_ok and loss_ok and not others
    summary = (f"spacing {actual_ns:.1f} ns; edges "
               f"{[n for n, _ in bursts]}; "
               f"others={sorted(others) or 'none'}"
               + ("" if mode.name != "text"
                  else f"; ovc={ovc} ovf={ovf}"))
    return ok, summary, diag


def phase_polarity_burst(mode, sg, tic, channel, ncyc=4000,
                         spacing_ns=50):
    """Max-rate BOTH burst, repeated POLARITY_BURST_REPEATS times: this
    is the phase that caught the rare seconds-rollover merge-order
    race, which only showed once every few bursts -- a single shot can
    pass by luck, so the regression hammers it every run."""
    expected = 2 * ncyc
    print(f"\n=== polarity burst x{POLARITY_BURST_REPEATS}: {ncyc} "
          f"pulses ({expected} edges) BOTH @{spacing_ns} ns "
          f"[{mode.name}] ===")
    ok = True
    for i in range(POLARITY_BURST_REPEATS):
        one_ok, summary, diag = _polarity_burst_once(
            mode, sg, tic, channel, ncyc, spacing_ns, expected)
        tag = "PASS" if one_ok else "FAIL"
        print(f"  [{tag}] iter {i + 1}/{POLARITY_BURST_REPEATS}: "
              f"{summary}")
        if diag:
            print(f"    [DIAG] {diag}")
        ok &= one_ok
    return expect(ok, f"all {POLARITY_BURST_REPEATS} max-rate BOTH "
                  f"bursts: complete, strictly alternating, no loss")


def phase_sustained(mode, sg, tic, channel):
    hz = SUSTAINED_HZ[mode.name]
    print(f"\n=== sustained {hz} Hz for {SUSTAINED_S:.0f}s "
          f"[{mode.name}] ===")
    tic.set_slope(channel, "POS")
    tic.set_divider(channel, 1)
    # Continuous (non-burst) pulse train at the mode's rated rate.
    sg.periodic(hz, pulse_width_s=min(1e-6, 0.2 / hz))
    times, edges, others, ovc, ovf, _ = mode.capture(
        tic, channel, SUSTAINED_S)
    # Rate is measured from the captured records' own time span, not
    # count/window: the latter loses ~(marker-sync + end-of-window
    # drain) at both ends regardless of rate. A device that keeps up
    # streams at the true source rate with no gaps; one that can't
    # either drops chunks (binary: shows as a span-rate deficit and a
    # gap) or overflows (text: the '#' line, asserted via no_loss).
    n = len(times)
    span = (times[-1] - times[0]) if n >= 2 else 0
    obs_hz = (n - 1) / span if span > 0 else 0
    max_gap = max((times[i + 1] - times[i] for i in range(n - 1)),
                  default=0)
    nominal = 1.0 / hz
    print(f"  collected {n} on ch{channel}; observed "
          f"{obs_hz:.0f} Hz over {span:.3f}s; "
          f"max gap {max_gap * 1e6:.1f} µs (nominal "
          f"{nominal * 1e6:.1f}); others={sorted(others) or 'none'}")

    ok = expect(n >= 100 and abs(obs_hz - hz) < 0.02 * hz,
                f"observed rate within 2% of {hz} Hz")
    ok &= expect(max_gap < 5 * nominal,
                 f"no streaming gap (max < {5 * nominal * 1e6:.0f} µs)")
    ok &= expect_no_loss(mode, ovc, ovf)
    ok &= expect(bool(times) and all(e == "+" for e in edges),
                 "every edge is rising '+'")
    ok &= expect(not others, "no spurious other-channel activity")
    sg.periodic(PULSE_HZ)  # restore the slow default for later phases
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
                      ("INP0:DIV notanumber", "-104")]:
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


def phase_single_pulse(sg, tic, channel, n=5):
    """A lone, isolated pulse must reach the host promptly -- not be
    held back waiting for more activity. Drives one 50 ns pulse per
    second (siggen burst, ncyc=1, internal 1 s trigger) and times each
    record's host arrival. With prompt flushing each pulse arrives in
    its own ~1 s slot; starvation/batching (e.g. a guard that waits
    for the frontier to move further) shows up as missing pulses or
    clumped arrivals."""
    print("\n=== single isolated pulse: timely delivery ===")
    tic.set_slope(channel, "POS")
    tic.set_divider(channel, 1)
    tic.send("FORM:DATA TEXT")
    tic.set_stream_enabled(True)
    sg.pulse_burst(1000, ncyc=1)  # one pulse, repeating every 1.000 s
    tic.discard_pending()

    arrivals, raw, seen = [], b"", 0
    start = time.monotonic()
    while time.monotonic() - start < n + 2.5 and len(arrivals) < n + 1:
        chunk = tic.read_raw(0.05)
        if not chunk:
            continue
        raw += chunk
        times, *_ = parse_text_stream(raw, channel)
        while seen < len(times):
            arrivals.append(time.monotonic())
            seen += 1
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
    _cli(port, "slope", "0", "BOTH")
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
    line_re = re.compile(r"^\d+ \d+\.\d{9} [+-]$")
    good = [ln for ln in lines if line_re.match(ln)]
    edges = [ln.split()[2] for ln in good]
    print(f"  `tsctl stream`: {len(lines)} lines, {len(good)} well-formed")
    ok &= expect(len(good) >= 5 and len(good) == len(lines),
                 "all stream lines are '<ch> <sec>.<ns> <+/->'")
    ok &= expect(bool(edges) and
                 all(edges[i] != edges[i + 1]
                     for i in range(len(edges) - 1)),
                 "CLI stream polarity strictly alternates")
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
        sg.periodic(PULSE_HZ)
        tic.reset()
        tic.discard_pending()
        print(f"Connected: {tic.idn()}")

        try:
            for mode in MODES:
                results.append(
                    (f"rising [{mode.name}]",
                     phase_rising(mode, tic, args.channel,
                                  args.duration)))
                results.append(
                    (f"both [{mode.name}]",
                     phase_both(mode, tic, args.channel, args.duration)))
                results.append(
                    (f"divider [{mode.name}]",
                     phase_divider(mode, tic, args.channel,
                                   args.duration)))
                results.append(
                    (f"output gating [{mode.name}]",
                     phase_output_gating(mode, tic, args.channel)))
                results.append(
                    (f"burst [{mode.name}]",
                     phase_burst(mode, sg, tic, args.channel)))
                results.append(
                    (f"polarity burst [{mode.name}]",
                     phase_polarity_burst(mode, sg, tic, args.channel)))
                results.append(
                    (f"sustained [{mode.name}]",
                     phase_sustained(mode, sg, tic, args.channel)))

            results.append(("scpi errors", phase_scpi_errors(tic)))
            results.append(("reset", phase_reset(tic, args.channel)))
            results.append(("single pulse",
                            phase_single_pulse(sg, tic, args.channel)))
            results.append(("idn serial", phase_idn_serial(tic)))
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
