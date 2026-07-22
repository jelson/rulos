#!/usr/bin/env python3

"""LectroTIC-4 hardware regression suite.

Every phase is a registered, self-contained statement of intent: it fully configures its own source
signal and device state (so phase order can never poison it), captures through the one shared
capture path, and scores with the shared zero-tolerance vocabulary in tstest.py. The @phase
decorator is the single source of each phase's label, wire parameterization (text/binary or
run-once), and runner needs (port release, BMP).

Hardware setup: a Lectrobox PG-4 wired straight through (PG-4 ch N -> LectroTIC-4 jack N, all four),
both devices on USB and on the same 10 MHz reference.

Usage:
  regression_test.py                       # all four channels
  regression_test.py --channel 1           # one channel only
  regression_test.py --phase rising --skip binary
  regression_test.py --skip ch3            # phase labels include [chN wire]"""

import argparse
import glob
import os
import random
import re
import statistics
import subprocess
import sys
import threading
import time
from types import SimpleNamespace

import serial

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "util"))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "pulsegen", "util"))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "util"))

import bmpflash
from pgctl import Pulsegen
import tsctl
from tsctl import LectroTIC4
import tstest

# Base test signal: 2 kHz is comfortably above the PG-4's ~1.9 kHz HRTIM floor and slow enough that
# every wire format keeps up with huge margin, so the per-feature phases test features, not
# throughput (the sustained phases own throughput).
PULSE_HZ = 2000
PULSE_WIDTH_S = 5e-6
PERIOD_NS = tstest.NS // PULSE_HZ
PULSE_WIDTH_NS = int(PULSE_WIDTH_S * tstest.NS)

WIRES = ("text", "binary")


# --------------------------------------------------------------------
# Phase registry: the decorator is the single source of each phase's label, wire parameterization,
# and runner needs.
# --------------------------------------------------------------------

PHASES = []


class PhaseSkip(Exception):
    """Raised by a phase that cannot run on this rig (e.g. optional hardware absent). Recorded as
    SKIP in the summary -- visible, never a vacuous pass."""


def phase(label, wires=WIRES, releases_port=False, needs_bmp=False, per_channel=None, **params):
    """Register the decorated function as a phase. One function can be registered several times with
    different `params` (bound as keyword arguments at run time), so a parameterized test reads as
    one visible spec plus a list of declared variants. Decorators stack bottom-up: the bottom
    registration runs first.

    per_channel phases run once per selected input channel (ctx.channel). Wire-parameterized phases
    are per-channel by definition -- they all drive ctx.channel through the shared capture path --
    so the flag defaults to True for them and False for run-once (wires=None) phases; pass it
    explicitly only to override (a run-once phase that reads ctx.channel meaningfully)."""

    def register(fn):
        PHASES.append(
            SimpleNamespace(
                label=label,
                fn=fn,
                wires=wires,
                releases_port=releases_port,
                needs_bmp=needs_bmp,
                per_channel=per_channel if per_channel is not None else wires is not None,
                params=params,
            )
        )
        return fn

    return register


# --------------------------------------------------------------------
# Per-wire phases
# --------------------------------------------------------------------


@phase("divider hw16", slope="NEG", divider=16)
@phase("divider hw8", slope="POS", divider=8)
@phase("divider", slope="POS", divider=5)
@phase("falling", slope="NEG", divider=1)
@phase("rising", slope="POS", divider=1)
def phase_periodic(ctx, slope, divider):
    """Single-slope periodic capture: the device must report exactly one record per `divider` source
    pulses, each at the right cadence with the right hardware-latched polarity, and nothing else."""
    ctx.src.periodic(ctx.channel, PULSE_HZ, PULSE_WIDTH_S)
    tstest.configure(ctx.tic, slopes={ctx.channel: slope}, dividers={ctx.channel: divider})
    cap = tstest.capture(ctx.tic, ctx.wire, ctx.duration)

    times = cap.times(ctx.channel)
    expected = PULSE_HZ / divider * ctx.duration
    tstest.report_collected(cap, ctx.channel, expected)
    tstest.report_intervals(times)

    tol = tstest.count_tol(PULSE_HZ / divider, 2 if divider == 1 else 1)
    ctx.ph.expect(
        abs(len(times) - expected) <= tol, f"count within {tol} of expected {expected:.1f}"
    )
    tstest.expect_no_loss(ctx.ph, cap)
    tstest.expect_quiet_others(ctx.ph, cap, {ctx.channel})
    tstest.expect_polarity(ctx.ph, cap.pols(ctx.channel), "+" if slope == "POS" else "-")
    tstest.expect_cadence(ctx.ph, times, PERIOD_NS * divider, "intervals")


@phase("both")
def phase_both(ctx):
    """BOTH-edge capture of a pulse train: every pulse contributes a rising and a falling edge.
    After host-side time sorting (the two sub-streams arrive in independent batches), gaps must
    strictly alternate short (pulse width) / long (period minus width), the polarity column must
    strictly alternate +/- in lockstep with the gap classes, and every interior gap of each class is
    held to the standard tolerance -- one missed edge anywhere breaks alternation or cadence."""
    ctx.src.periodic(ctx.channel, PULSE_HZ, PULSE_WIDTH_S)
    tstest.configure(ctx.tic, slopes={ctx.channel: "BOTH"})
    cap = tstest.capture(ctx.tic, ctx.wire, ctx.duration)
    ph = ctx.ph

    recs = cap.records(ctx.channel)
    expected = 2 * PULSE_HZ * ctx.duration
    tstest.report_collected(cap, ctx.channel, expected)
    if len(recs) < 4:
        ph.expect(False, "enough samples for interval analysis")
        return
    tstest.expect_substreams_monotonic(ph, recs)
    n_raw = len(recs)
    recs = tstest.trim_window(sorted(recs))
    times = [t for t, _ in recs]
    pols = [p for _, p in recs]

    deltas = [b - a for a, b in zip(times, times[1:])]
    cls, short, long = tstest.classify_gaps(deltas, PERIOD_NS // 2)
    long_target = PERIOD_NS - PULSE_WIDTH_NS
    # The capture window opens/closes asynchronously to the pulse train, so the first and last gap
    # can straddle a pulse whose other edge lay outside the window; per-gap exactness is asserted on
    # the interior, while alternation and the count span the whole sequence and catch any dropped
    # interior edge.
    _, ishort, ilong = tstest.classify_gaps(deltas[1:-1], PERIOD_NS // 2)
    print(f"  short(rise→fall) n={len(short)}; long(fall→rise) n={len(long)}")

    tol = tstest.count_tol(2 * PULSE_HZ, 4)
    ph.expect(abs(n_raw - expected) <= tol, f"count within {tol} of expected {expected:.0f}")
    tstest.expect_no_loss(ph, cap)
    tstest.expect_quiet_others(ph, cap, {ctx.channel})
    ph.expect(abs(len(short) - len(long)) <= 1, "short/long delta counts roughly equal")
    ph.expect(
        len(cls) >= 4 and all(cls[i] != cls[i + 1] for i in range(len(cls) - 1)),
        "gaps strictly alternate short/long (every pulse = 2 edges, regardless of start)",
    )
    ph.expect(
        all(pols[i] != pols[i + 1] for i in range(len(pols) - 1)),
        "polarity strictly alternates +/-",
    )
    ph.expect(
        all((cls[i] == "S") == (pols[i] == "+") for i in range(len(cls))),
        "every short gap is rise->fall ('+' then '-'), every long gap fall->rise",
    )
    ph.expect(bool(ishort), "interior short gaps present")
    tstest.expect_gaps(ph, ishort, PULSE_WIDTH_NS, "short(rise→fall)")
    ph.expect(bool(ilong), "interior long gaps present")
    tstest.expect_gaps(ph, ilong, long_target, "long(fall→rise)")


@phase("output gating")
def phase_output_gating(ctx):
    """OUTPut:STATe OFF must silence the wire completely -- not one byte -- while capture continues
    into the internal buffer; ON must deliver the silenced-period backlog (the documented
    query-session workflow: up to 16,384 records buffer during a pause) followed seamlessly by live
    records. Perfect cadence across the whole capture proves not one silenced pulse was lost."""
    ctx.src.periodic(ctx.channel, PULSE_HZ, PULSE_WIDTH_S)
    tstest.configure(ctx.tic)
    # Select the wire format up front: the backlog read below must not issue the format command
    # (or any marker sync) itself, or it would wipe the backlog this test exists to verify.
    ctx.tic.send(f"FORM:DATA {'TEXT' if ctx.wire == 'text' else 'BIN'}")
    ctx.tic.set_stream_enabled(False)
    # Marker-synced barrier: drains the prior phase's in-flight residue end to end, so the silence
    # window that follows is deterministic regardless of what ran before.
    ctx.tic.discard_pending()
    bytes_off = ctx.tic.read_raw(1.0)
    ctx.ph.expect(len(bytes_off) == 0, "silent while OUTP:STAT OFF")

    ctx.tic.set_stream_enabled(True)
    cap = tstest.capture(ctx.tic, ctx.wire, 1.5, discard=False)
    times = cap.times(ctx.channel)
    # The capture must contain the ~1 s of silenced backlog PLUS the live window -- far more than
    # the live window alone could produce -- and be gap-free straight through the OFF/ON boundary.
    floor = int(PULSE_HZ * 2.2)
    print(
        f"  OFF: {len(bytes_off)} bytes (expect 0); after ON: {len(times)} stamps "
        f"(backlog + live; expect >= {floor})"
    )
    ctx.ph.expect(
        len(times) >= floor,
        f"silenced-period backlog delivered after OUTP:STAT ON (n={len(times)})",
    )
    tstest.expect_cadence(ctx.ph, times, PERIOD_NS, "backlog+live cadence", edges="interior")
    tstest.expect_no_loss(ctx.ph, cap)
    tstest.expect_quiet_others(ctx.ph, cap, {ctx.channel})


@phase("burst", ncyc=16383, spacing_ns=100)
def phase_burst(ctx, ncyc, spacing_ns):
    """A hardware-counted burst that fills the device's timestamp ring exactly (16383 pulses at 100
    ns): every burst that begins AND ends inside the capture window must contain every pulse -- a
    single drop anywhere in an interior burst fails."""
    tstest.configure(ctx.tic)
    actual_ns = ctx.src.burst(ctx.channel, spacing_ns, ncyc)
    cap = tstest.capture(ctx.tic, ctx.wire, 3.5)
    for c in cap.comments:
        print(f"    | {c}")

    times = cap.times(ctx.channel)
    sizes = [len(b) for b in tstest.split_bursts(times)]
    interior = sizes[1:-1]
    short = [(i + 1, n) for i, n in enumerate(interior) if n != ncyc]
    print(
        f"  spacing {actual_ns:.1f} ns; bursts {sizes}; "
        f"interior must all == {ncyc}; "
        f"short interior (idx,n)={short or 'none'}"
    )

    ctx.ph.expect(len(interior) >= 1, f">=1 fully-captured (interior) burst of {ncyc}")
    ctx.ph.expect(not short, f"every interior burst is exactly {ncyc} pulses (not one missed)")
    tstest.expect_no_loss(ctx.ph, cap)
    tstest.expect_quiet_others(ctx.ph, cap, {ctx.channel})
    tstest.expect_polarity(ctx.ph, cap.pols(ctx.channel), "+")
    tstest.expect_monotonic(ctx.ph, times)


@phase("overcapture", ncyc=20000)
def phase_overcapture(ctx, ncyc):
    """Deliberately overrun the device and confirm it *reports* the loss on this wire. Every other
    phase asserts zero loss; this is the only one that exercises the loss-report path -- the text '#
    ch<N>: ...' line and the binary PULSES_LOST record. A 20k-pulse burst (well past the
    16,384-record ring) at the PG-4's minimum spacing forces loss; at that spacing the device keeps
    up edge-for-edge, so the loss shows as buffer overflow rather than overcaptures. Either nonzero
    counter proves the report path."""
    tstest.configure(ctx.tic)
    # Request 1 ns; the PG-4 clamps it up to its ~48 ns floor.
    actual_ns = ctx.src.burst(ctx.channel, 1, ncyc)
    cap = tstest.capture(ctx.tic, ctx.wire, 3.5)
    for c in cap.comments:
        print(f"    | {c}")

    sizes = [len(b) for b in tstest.split_bursts(cap.times(ctx.channel))]
    print(
        f"  spacing {actual_ns:.1f} ns; bursts {sizes}; "
        f"overcaptures={cap.overcaptures}, buf overflows={cap.buf_overflows}"
    )

    ctx.ph.expect(sum(sizes) > 0, "device still streamed timestamps through the overrun")
    tstest.expect_quiet_others(ctx.ph, cap, {ctx.channel})
    tstest.expect_loss_only_for(ctx.ph, cap, ctx.channel)


@phase("polarity burst", ncyc=4096, spacing_ns=200)
def phase_polarity_burst(ctx, ncyc, spacing_ns):
    """BOTH-edge burst at tight spacing -- the paired-capture stress test, with both sub-streams at
    full burst rate at once: every interior burst has exactly 2*ncyc edges, each sub-stream arrives
    in order, and after a host-side time sort the polarity strictly alternates (every pulse
    contributes '+' then '-'). A single duplicate or dropped edge anywhere fails."""
    tstest.configure(ctx.tic, slopes={ctx.channel: "BOTH"})
    actual_ns = ctx.src.burst(ctx.channel, spacing_ns, ncyc)
    cap = tstest.capture(ctx.tic, ctx.wire, 3.5)
    for c in cap.comments:
        print(f"    | {c}")

    recs = cap.records(ctx.channel)
    sizes = [len(b) for b in tstest.split_bursts([t for t, _ in recs])]
    interior = sizes[1:-1]
    bad = [(i + 1, n) for i, n in enumerate(interior) if n != 2 * ncyc]
    print(
        f"  spacing {actual_ns:.1f} ns; bursts {sizes}; interior must all == {2 * ncyc}; "
        f"bad interior (idx,n)={bad or 'none'}"
    )

    ctx.ph.expect(len(interior) >= 1, f">=1 fully-captured (interior) burst of {2 * ncyc} edges")
    ctx.ph.expect(
        not bad, f"every interior burst is exactly {2 * ncyc} edges (both edges of every pulse)"
    )
    tstest.expect_no_loss(ctx.ph, cap)
    tstest.expect_quiet_others(ctx.ph, cap, {ctx.channel})
    tstest.expect_substreams_monotonic(ctx.ph, recs)
    spols = [p for _, p in tstest.trim_window(sorted(recs))]
    ctx.ph.expect(
        all(spols[i] != spols[i + 1] for i in range(len(spols) - 1)),
        "polarity strictly alternates through every burst (after host-side time sort)",
    )


# Sustained rate per wire format: the device's rated throughput specs.
SUSTAINED_HZ = {"text": 25000, "binary": 100000}
SUSTAINED_S = 5.0


@phase("sustained")
def phase_sustained(ctx):
    """Continuous full-rate streaming at the wire's rated sustained rate: every interior gap must
    equal the source period -- not a single pulse missed, no excuses."""
    hz = SUSTAINED_HZ[ctx.wire]
    ctx.src.periodic(ctx.channel, hz, min(1e-6, 0.2 / hz))
    tstest.configure(ctx.tic)
    cap = tstest.capture(ctx.tic, ctx.wire, SUSTAINED_S, settle_s=0.5)

    times = cap.times(ctx.channel)
    n = len(times)
    span_ns = (times[-1] - times[0]) if n >= 2 else 0
    obs_hz = (n - 1) * tstest.NS / span_ns if span_ns else 0
    tstest.report_collected(cap, ctx.channel, hz * SUSTAINED_S)
    print(f"  observed {obs_hz:.0f} Hz over {span_ns / 1e9:.3f}s")

    ctx.ph.expect(n >= 100, f"sustained {hz} Hz produced enough records")
    tstest.expect_cadence(ctx.ph, times, tstest.NS // hz, "sustained gap", edges="interior")
    tstest.expect_no_loss(ctx.ph, cap)
    tstest.expect_quiet_others(ctx.ph, cap, {ctx.channel})
    tstest.expect_polarity(ctx.ph, cap.pols(ctx.channel), "+")


@phase("single pulse", n=5)
def phase_single_pulse(ctx, n):
    """A lone, isolated pulse must reach the host promptly -- not be held back waiting for more
    activity. One pulse per second; each record's host arrival is timed through this wire's path, so
    each must land in its own ~1 s slot."""
    tstest.configure(ctx.tic)
    ctx.src.burst(ctx.channel, 1000, 1)  # one pulse, repeating at 1 s
    arr = tstest.arrivals(ctx.tic, ctx.wire, ctx.channel, n + 2.5, n + 1)
    ctx.src.off()

    deltas = [b - a for a, b in zip(arr, arr[1:])]
    print(
        f"  received {len(arr)} isolated pulses; host "
        f"inter-arrival: {[f'{d:.2f}' for d in deltas]}"
    )
    ctx.ph.expect(abs(len(arr) - n) <= 1, f"~{n} isolated pulses received (none starved)")
    if deltas:
        ctx.ph.expect(
            max(deltas) < 1.3,
            f"each pulse delivered in its own ~1 s slot (max gap {max(deltas):.2f}s < 1.3s)",
        )
        ctx.ph.expect(
            min(deltas) > 0.7, f"no pulses batched together (min gap {min(deltas):.2f}s > 0.7s)"
        )


# --------------------------------------------------------------------
# Run-once phases
# --------------------------------------------------------------------


@phase("scpi errors", wires=None)
def phase_scpi_errors(ctx):
    """Bad commands must latch their documented SCPI errors; SYST:ERR? reads-and-clears; good
    commands don't latch anything; the device stays responsive throughout."""
    tic = ctx.tic
    # Clean text slate: a preceding binary phase can leave undrained binary bytes that readline()
    # would mis-read as the first error response. The marker sync flushes both sides.
    tic.send("FORM:DATA TEXT")
    tic.set_stream_enabled(False)
    tic.discard_pending()
    tic.clear_errors()
    ctx.ph.expect(tic.get_error().startswith("0,"), "SYST:ERR? after *CLS reports no-error")
    for cmd, code in [
        ("INP0:DIV 0", "-222"),
        ("INP0:SLOP FROBNICATE", "-104"),
        ("FOOBAR", "-100"),
        ("INP0:DIV notanumber", "-104"),
        ("INP4:SLOP POS", "-100"),  # ch 4 out of 0..3
    ]:
        tic.clear_errors()
        tic.reset_input_buffer()
        tic.send(cmd)
        err = tic.get_error()
        ctx.ph.expect(code in err, f"`{cmd}` -> {code} (got {err!r})")
    ctx.ph.expect(tic.get_error().startswith("0,"), "SYST:ERR? auto-clears after read")
    ctx.ph.expect(tic.idn().startswith("Lectrobox,LectroTIC-4"), "device still answers *IDN?")


@phase("serial ctl", wires=None)
def phase_serial_ctl(ctx):
    """Auxiliary serial input control protocol: off by default, baud/state set+query roundtrips,
    range validation, *RST restore. (The data path needs a serial source on PA10, which the
    automated rig doesn't have.)"""
    tic = ctx.tic
    ph = ctx.ph
    tic.send("FORM:DATA TEXT")
    tic.set_stream_enabled(False)
    tic.discard_pending()
    tic.send("*RST")
    time.sleep(0.3)
    ph.expect(not tic.get_serial_enabled(), "serial input OFF by default after *RST")
    ph.expect(tic.get_serial_baud() == 115200, "default baud 115200")

    tic.set_serial_baud(9600)
    ph.expect(tic.get_serial_baud() == 9600, "baud set+query roundtrip (9600)")
    tic.set_serial_enabled(True)
    ph.expect(tic.get_serial_enabled(), "SER:STAT ON roundtrip")

    tic.clear_errors()
    tic.set_serial_baud(99)
    err = tic.get_error()
    ph.expect("-222" in err, f"baud 99 rejected with -222 (got {err!r})")
    ph.expect(tic.get_serial_baud() == 9600, "rejected baud leaves setting untouched")
    tic.clear_errors()

    tic.set_serial_enabled(False)
    ph.expect(not tic.get_serial_enabled(), "SER:STAT OFF roundtrip")

    tic.set_serial_baud(230400)
    tic.set_serial_enabled(True)
    tic.send("*RST")
    time.sleep(0.3)
    ph.expect(
        not tic.get_serial_enabled() and tic.get_serial_baud() == 115200,
        "*RST restores serial defaults (off, 115200)",
    )
    ph.expect(tic.idn().startswith("Lectrobox,LectroTIC-4"), "device still answers *IDN?")


@phase("serial data", wires=None)
def phase_serial_data(ctx):
    """End-to-end serial input: lines driven into PA10 by the BMP's auxiliary UART come back as
    timestamped records -- intact at the binary framing's 8-byte group boundaries, in order, no
    loss, stamped at the send cadence -- while a pulse channel streams alongside, since sharing
    one timeline with pulses is the feature's whole point: the pulse stream must stay
    gap-perfect with serial payloads interleaved through it, and vice versa. Skips when the BMP
    aux UART is absent or not wired to this unit's serial input."""
    uarts = glob.glob("/dev/serial/by-id/*Black_Magic*-if02")
    if not uarts:
        raise PhaseSkip("no BMP auxiliary UART on this host")
    tic = ctx.tic
    ph = ctx.ph

    tic.set_serial_baud(115200)
    tic.set_serial_enabled(True)
    tic.discard_pending()
    src = serial.Serial(uarts[0], 115200, timeout=1)
    try:
        # Wiring probe: if the first line never arrives, the UART isn't cabled to this unit.
        # Sent on a delay so it lands inside read_for's marker-synced window.
        probe = threading.Timer(0.5, lambda: (src.write(b"probe\n"), src.flush()))
        probe.start()
        found = any(isinstance(r, tsctl.SerialLine) for r in tic.read_for(2.0))
        probe.join()
        if not found:
            raise PhaseSkip("BMP aux UART TX is not wired to this unit's serial input (PA10)")

        # Pulse traffic on one channel for the whole capture: serial records must interleave
        # with a live timestamp stream without disturbing it.
        pulse_hz = 5000
        ctx.src.periodic(ctx.channel, pulse_hz, PULSE_WIDTH_S)
        tstest.configure(tic, slopes={ctx.channel: "POS"}, dividers={ctx.channel: 1})

        # Lengths straddling the (1 + len) 8-byte payload-group boundaries, a long line, then a
        # periodic tail so the timestamp check sees a decent train.
        lines = ["a", "1234567", "12345678", "123456ab7890123", "123456ab78901234", "x" * 255]
        lines += [f"periodic {i}" for i in range(6)]
        spacing_s = 0.15
        tic.discard_pending()

        host_times = []

        def send():
            time.sleep(0.4)
            for l in lines:
                host_times.append(time.monotonic())
                src.write((l + "\n").encode())
                src.flush()
                time.sleep(spacing_s)

        sender = threading.Thread(target=send)
        sender.start()
        got, lost, pulses = [], 0, []
        window_s = 0.4 + spacing_s * len(lines) + 1.0
        for r in tic.read_for(window_s):
            if isinstance(r, tsctl.SerialLine):
                got.append(r)
            elif isinstance(r, tsctl.SerialLinesLost):
                lost += r.count
            elif isinstance(r, tsctl.Timestamp) and r.channel == ctx.channel:
                pulses.append(r.seconds * tstest.NS + r.nanoseconds)
        sender.join()

        ph.expect(
            len(pulses) > 0.8 * pulse_hz * window_s,
            f"pulse stream flowed throughout ({len(pulses)} records at {pulse_hz} Hz)",
        )
        tstest.expect_cadence(
            ph, pulses, tstest.NS // pulse_hz, "pulse cadence with serial interleaved"
        )

        ph.expect([r.text for r in got] == lines, f"all {len(lines)} lines intact and in order")
        ph.expect(lost == 0, "no lines dropped")
        times = [r.seconds * tstest.NS + r.nanoseconds for r in got]
        ph.expect(all(b > a for a, b in zip(times, times[1:])), "timestamps strictly increasing")
        if len(got) == len(lines):
            # Device timestamps must agree with the host's independent clock, interval by
            # interval. Budget: host sleep/USB jitter plus the device's task-level stamping
            # latency -- a wrong timebase or misassembled seconds/ticks blows this immediately.
            budget_s = 0.03
            errs = [
                (dt - dh)
                for (a, b, ha, hb) in zip(times, times[1:], host_times, host_times[1:])
                for (dt, dh) in [((b - a) / 1e9, hb - ha)]
            ]
            worst = max(abs(e) for e in errs)
            ph.expect(
                worst < budget_s,
                f"device intervals match the host clock within {budget_s * 1000:.0f} ms "
                f"(worst {worst * 1000:+.1f} ms over {len(errs)} intervals)",
            )
    finally:
        src.close()
        ctx.src.off()
        tic.set_serial_enabled(False)


@phase("reset", wires=None)
def phase_reset(ctx):
    """*RST must restore every channel to defaults (POS, divider 1) and re-enable streaming, from a
    fully scrambled configuration."""
    tic = ctx.tic
    tstest.configure(
        tic, slopes={c: "BOTH" for c in tstest.CHANNELS}, dividers={c: 7 for c in tstest.CHANNELS}
    )
    tic.set_stream_enabled(False)
    tic.reset_input_buffer()
    ctx.ph.expect(tic.get_slope(ctx.channel) == "BOTH", "pre-RST slope BOTH")
    ctx.ph.expect(tic.get_divider(ctx.channel) == "7", "pre-RST divider 7")
    ctx.ph.expect(tic.get_stream_enabled() is False, "pre-RST stream off")

    tic.reset()
    tic.reset_input_buffer()
    for ch in tstest.CHANNELS:
        ctx.ph.expect(tic.get_slope(ch) == "POS", f"ch{ch} slope -> POS")
        ctx.ph.expect(tic.get_divider(ch) == "1", f"ch{ch} divider -> 1")
    ctx.ph.expect(tic.get_stream_enabled() is True, "post-RST stream on")


@phase("slope switch", wires=None, per_channel=True)
def phase_slope_switch(ctx):
    """Switch POS -> NEG mid-capture and confirm the device starts detecting the opposite edge --
    coverage the steady POS/BOTH phases don't give. Capturing rising edges gives period-long gaps;
    after SLOP NEG, falling edges, also period apart; between them sits exactly ONE crossover gap of
    width + k*period (k = whole periods until the command takes effect; the host can't phase-lock
    that, so k just has to be small relative to its command latency budget). Mode-independent device
    behavior, so it runs once (text wire)."""
    tic = ctx.tic
    width_ns = PERIOD_NS // 10
    ctx.src.periodic(ctx.channel, PULSE_HZ, width_ns / tstest.NS)
    tstest.configure(tic)
    tic.send("FORM:DATA TEXT")
    tic.set_stream_enabled(True)
    tic.discard_pending()

    # Enough edges that healthy runs surround the switch at any rate.
    want = max(30, int(3 * PULSE_HZ))
    times, pols, raw, seen, switched = [], [], b"", 0, False
    start = time.monotonic()
    while time.monotonic() - start < 6.0 and len(times) < want:
        chunk = tic.read_raw(0.05)
        if chunk:
            raw += chunk
            cap = tstest.decode_text(raw)
            recs = cap.records(ctx.channel)
            while seen < len(recs):
                times.append(recs[seen][0])
                pols.append(recs[seen][1])
                seen += 1
        if not switched and len(times) >= want // 3:
            tic.send(f"INP{ctx.channel}:SLOP NEG")  # POS -> NEG mid-stream
            switched = True
    ctx.src.off()

    # The period/20 split only separates the lone crossover gap from the steady run -- it is not the
    # timing check. Once separated, the steady gaps (rising-rising before, falling-falling after)
    # are held to GAP_TOL_NS of the period like every other phase, and the crossover to GAP_TOL_NS
    # of width + k*period; only k (whole periods until the host's command lands) is
    # non-deterministic.
    deltas = [b - a for a, b in zip(times, times[1:])]
    steady_idx = [i for i, d in enumerate(deltas) if abs(d - PERIOD_NS) < PERIOD_NS // 20]
    anomalies = [(i, d) for i, d in enumerate(deltas) if i not in steady_idx]
    steady_gaps = [deltas[i] for i in steady_idx]
    print(
        f"  collected {len(times)} edges, {len(deltas)} gaps; anomalies (idx, ms): "
        f"{[(i, round(d / 1e6, 1)) for i, d in anomalies]}"
    )

    ctx.ph.expect(switched, "INP:SLOP NEG sent mid-capture")
    ctx.ph.expect(len(times) >= want * 2 // 3, f">={want * 2 // 3} edges across the switch")
    ctx.ph.expect(
        len(anomalies) == 1,
        "exactly one (clean) transition gap -- a string of ~100 ms, one crossover, "
        "~100 ms again",
    )
    ctx.ph.expect(bool(steady_gaps), "steady gaps present")
    tstest.expect_gaps(ctx.ph, steady_gaps, PERIOD_NS, "steady (POS then NEG)")
    # The polarity column must tell the same story: a run of '+', one flip, a run of '-' -- with
    # the flip at the crossover gap.
    flips = [i for i in range(len(pols) - 1) if pols[i] != pols[i + 1]]
    ctx.ph.expect(
        len(flips) == 1 and pols[0] == "+" and pols[-1] == "-",
        f"polarity flips '+'->'-' exactly once (flips at {flips})",
    )
    if anomalies:
        i, d = anomalies[0]
        if len(flips) == 1:
            ctx.ph.expect(
                flips[0] == i,
                f"polarity flip (gap #{flips[0]}) coincides with the crossover gap (#{i})",
            )
        k = round((d - width_ns) / PERIOD_NS)
        # Command latency budget: ~50 ms of host->device delay, in whole periods of the source rate.
        kmax = max(3, int(0.05 * PULSE_HZ))
        ctx.ph.expect(0 <= k <= kmax, f"crossover within the latency budget (k={k} <= {kmax})")
        tstest.expect_gaps(ctx.ph, [d], width_ns + k * PERIOD_NS, "crossover")
        ctx.ph.expect(
            i >= 5 and i <= len(deltas) - 5,
            "healthy ~100 ms runs before AND after the switch (NEG detection working)",
        )


@phase("idn serial", wires=None)
def phase_idn_serial(ctx):
    """The *IDN? serial field must equal the USB descriptor serial: the LT4- product tag plus the
    24-hex-digit 96-bit chip UID."""
    tic = ctx.tic
    tic.set_stream_enabled(False)
    tic.discard_pending()
    idn = tic.idn()
    parts = idn.split(",")
    idn_serial = parts[2] if len(parts) == 4 else None
    usb_serial = tic.usb_serial
    print(f"  {idn!r}; usb={usb_serial!r}")
    ctx.ph.expect(len(parts) == 4, "*IDN? has 4 fields")
    ctx.ph.expect(bool(usb_serial), "USB descriptor serial readable")
    ctx.ph.expect(idn_serial == usb_serial, f"*IDN? serial == USB serial ({idn_serial!r})")
    ctx.ph.expect(
        bool(idn_serial) and idn_serial.startswith("LT4-") and len(idn_serial) == len("LT4-") + 24,
        f"serial is 'LT4-' + 24-hex UID ({idn_serial!r})",
    )


@phase("divider semantics", wires=None, per_channel=True)
def phase_divider_semantics(ctx):
    """Two fine points of the divider contract. In BOTH mode the divider counts EDGES -- each
    pulse contributes its rising and falling edge -- so divider N yields 2*rate/N records with
    both polarities surviving. And setting the divider resets its progress counter: re-setting it
    faster than N source pulses can accumulate keeps the channel silent indefinitely, because no
    partial count survives a set."""
    ph = ctx.ph

    # (a) BOTH mode: the divider counts edges, not pulses.
    ctx.src.periodic(ctx.channel, PULSE_HZ, PULSE_WIDTH_S)
    tstest.configure(ctx.tic, slopes={ctx.channel: "BOTH"}, dividers={ctx.channel: 5})
    cap = tstest.capture(ctx.tic, "binary", 5.0)
    recs = cap.records(ctx.channel)
    expected = 2 * PULSE_HZ * 5.0 / 5
    tol = tstest.count_tol(2 * PULSE_HZ / 5, 2)
    npos = sum(1 for _, p in recs if p == "+")
    print(
        f"  BOTH+div5: {len(recs)} records (expect ~{expected:.0f}); {npos} '+', "
        f"{len(recs) - npos} '-'"
    )
    ph.expect(
        abs(len(recs) - expected) <= tol,
        f"one record per 5 EDGES in BOTH mode (count within {tol} of {expected:.0f})",
    )
    ph.expect(
        abs(npos - len(recs) / 2) <= len(recs) * 0.05 + 5,
        "both polarities survive the divider (~half each)",
    )
    tstest.expect_substreams_monotonic(ph, recs)
    tstest.expect_no_loss(ph, cap)
    tstest.expect_quiet_others(ph, cap, {ctx.channel})

    # (b) Setting the divider resets its counter. With divider == one second of pulses, re-setting
    # every ~0.2 s means the count never reaches the divider: the channel must stay silent the
    # whole time. Stop re-setting and the records resume -- proving the silence was the reset
    # semantics, not a dead channel.
    tstest.configure(ctx.tic, dividers={ctx.channel: PULSE_HZ})  # 1 record per second
    ctx.tic.send("FORM:DATA BIN")
    ctx.tic.set_stream_enabled(True)
    ctx.tic.discard_pending()
    for _ in range(12):
        time.sleep(0.2)
        ctx.tic.set_divider(ctx.channel, PULSE_HZ)  # reset the count before it can reach 1 s
    cap = tstest.capture(ctx.tic, "binary", 0.3, discard=False)
    ph.expect(
        len(cap.times(ctx.channel)) == 0,
        f"silent while the divider is re-set faster than it can fill "
        f"({len(cap.times(ctx.channel))} records in 2.4 s of spam)",
    )
    cap = tstest.capture(ctx.tic, "binary", 2.5, discard=False)
    n = len(cap.times(ctx.channel))
    ph.expect(n >= 1, f"records resume once the re-setting stops (n={n} in 2.5 s at 1/s)")


# 4-channel SYNC timing: 200 us period (5 kHz/channel), firing-cluster split at half a period.
PHASE_LOCK_PERIOD_NS = 200_000
PHASE_LOCK_SPLIT_NS = PHASE_LOCK_PERIOD_NS // 2
TIM5_GROUP = (1, 3)


def _capture_firings(ctx, period_ns, delays_ns, duration_s):
    """Shared 4-channel SYNC measurement: drive all four channels phase-locked with the given
    per-channel delays, capture on the binary wire, and return (complete_firings, median_offsets)
    with the universal strictness applied: no loss, every interior firing complete on all four
    channels, every firing exactly one period apart, and no single firing's offsets straying from
    the medians."""
    ctx.src.sync(delays_ns, period_ns)
    time.sleep(0.1)
    cap = tstest.capture(ctx.tic, "binary", duration_s)
    ph = ctx.ph
    counts = {c: len(cap.records(c)) for c in tstest.CHANNELS}
    print(f"  records per channel: {counts}")
    ph.expect(
        all(counts[c] > 50 for c in tstest.CHANNELS), f"all four channels produced records {counts}"
    )
    tstest.expect_no_loss(ph, cap)
    complete, interior_bad = tstest.firings(cap, PHASE_LOCK_SPLIT_NS)
    if not complete:
        ph.expect(False, "firings paired on all 4 channels")
        return None, None
    ph.expect(
        interior_bad == 0,
        f"every interior firing complete on all four channels ({interior_bad} incomplete)",
    )
    gaps = [b[0] - a[0] for a, b in zip(complete, complete[1:])]
    tstest.expect_gaps(ph, gaps, period_ns, f"firing cadence ({len(gaps)} gaps)")
    med = tstest.median_offsets(complete)
    tstest.expect_firing_spread(ph, complete, med)
    return complete, med


@phase("phase lock", wires=None)
def phase_phase_lock(ctx):
    """SYNC, all delays 0: four coincident edges. Every channel must timestamp the same instant;
    the offset between the TIM5 group (jacks 1/3) and the TIM2 group (jacks 0/2) is the phase-lock
    metric, expected within ~1 tick of zero."""
    tstest.configure(ctx.tic)
    ctx.src.off()
    _, med = _capture_firings(ctx, PHASE_LOCK_PERIOD_NS, {c: 0 for c in tstest.CHANNELS}, 2.0)
    if med is None:
        return
    print("  offset from ch0 (ns): " + ", ".join(f"ch{c}={med[c]:+.1f}" for c in tstest.CHANNELS))
    tim5 = statistics.median([med[c] for c in TIM5_GROUP])
    ctx.ph.expect(abs(med[2]) <= 8, f"ch2 coincident with ch0 (both TIM2): {med[2]:+.1f} ns")
    ctx.ph.expect(
        abs(med[3] - med[1]) <= 8, f"ch3 coincident with ch1 (both TIM5): {med[3] - med[1]:+.1f} ns"
    )
    ctx.ph.expect(
        abs(tim5) <= 8,
        f"TIM5 phase-locked to TIM2 (group offset {tim5:+.1f} ns; a small fixed offset is "
        f"calibratable trigger latency, large/variable is a sync fault)",
    )


@phase("stair 4ns", wires=None, step_ns=4, base_ns=1000)
@phase("stair", wires=None, step_ns=100, base_ns=0)
def phase_stair(ctx, step_ns, base_ns):
    """SYNC, per-channel delays base/base+step/base+2step/base+3step, measured DIFFERENTIALLY
    against a flat all-channels-at-base capture taken first in the same phase: each channel's
    baseline-corrected offset must equal its programmed step multiple to within one 4 ns tick. The
    flat baseline cancels the fixed per-channel systematics (TIM2<->TIM5 trigger latency,
    cable-length skew) that would otherwise force a tolerance wider than a fine staircase. The base
    delay rides under every channel equally; a fine stair needs base_ns > 0 because per-channel
    delays below the PG-4's minimum HRTIM compare are clamped and never appear on the wire."""
    tstest.configure(ctx.tic)
    ctx.src.off()
    _, baseline = _capture_firings(
        ctx, PHASE_LOCK_PERIOD_NS, {c: base_ns for c in tstest.CHANNELS}, 1.0
    )
    if baseline is None:
        return
    print(
        "  baseline offsets (ns): "
        + ", ".join(f"ch{c}={baseline[c]:+.1f}" for c in tstest.CHANNELS)
    )
    _, med = _capture_firings(
        ctx, PHASE_LOCK_PERIOD_NS, {c: base_ns + c * step_ns for c in tstest.CHANNELS}, 2.0
    )
    if med is None:
        return
    print(
        "  baseline-corrected offsets (ns): "
        + ", ".join(f"ch{c}={med[c] - baseline[c]:+.1f}" for c in tstest.CHANNELS)
    )
    for c in tstest.CHANNELS:
        delta = med[c] - baseline[c]
        ctx.ph.expect(
            abs(delta - c * step_ns) <= tstest.TICK_NS,
            f"ch{c} at programmed {c * step_ns} ns offset (baseline-corrected {delta:+.1f})",
        )


# Per-channel rate for the 4-channel sustained phase: every jack at once, summing to the binary
# wire's rated 100k records/s aggregate.
SUSTAINED_4CH_HZ = 25000


@phase("sustained 4ch", wires=None)
def phase_sustained_4ch(ctx):
    """All four channels streaming at once at the rated aggregate: 4 x 25 kHz = 100k records/s
    spread across every jack, exercising four concurrent DMA capture streams, the shared ring, and
    USB together at the spec'd binary sustained rate. Every channel's every interior gap must be
    the source period."""
    tstest.configure(ctx.tic)
    ctx.src.off()
    ctx.src.sync({c: 0 for c in tstest.CHANNELS}, tstest.NS // SUSTAINED_4CH_HZ)
    time.sleep(0.1)
    cap = tstest.capture(ctx.tic, "binary", SUSTAINED_S, settle_s=0.5)
    tstest.expect_no_loss(ctx.ph, cap)
    for c in tstest.CHANNELS:
        times = cap.times(c)
        ctx.ph.expect(len(times) >= 100, f"ch{c} produced records (n={len(times)})")
        if len(times) < 4:
            continue
        tstest.expect_cadence(
            ctx.ph,
            times,
            tstest.NS // SUSTAINED_4CH_HZ,
            f"ch{c} gaps (n={len(times)})",
            edges="interior",
        )
        tstest.expect_polarity(ctx.ph, cap.pols(c), "+")


# Four distinct observed rates from two PG-4 period domains (ch0/1 share Timer F, ch2/3 Timer E --
# four distinct source frequencies are physically impossible on this board) crossed with
# per-channel dividers, which also exercises divider correctness on concurrently active channels:
# 25k/1, 25k/2, 40k/1, 40k/4 = 25k, 12.5k, 40k, 10k.
MULTI_RATE_SRC_HZ = {0: 25000, 1: 25000, 2: 40000, 3: 40000}
MULTI_RATE_DIV = {0: 1, 1: 2, 2: 1, 3: 4}


@phase("multi rate", wires=None)
def phase_multi_rate(ctx):
    """Every channel at a different observed rate, all concurrent: two ASYNC source frequencies x
    per-channel dividers. Each channel must show exactly its own expected rate -- every interior
    gap is divider x source period, and the average rate matches to ppm."""
    tstest.configure(ctx.tic, dividers=MULTI_RATE_DIV)
    ctx.src.async_rates(MULTI_RATE_SRC_HZ[0], MULTI_RATE_SRC_HZ[2])
    time.sleep(0.1)
    cap = tstest.capture(ctx.tic, "binary", 5.0, settle_s=0.5)
    tstest.expect_no_loss(ctx.ph, cap)
    for c in tstest.CHANNELS:
        hz = MULTI_RATE_SRC_HZ[c] / MULTI_RATE_DIV[c]
        times = cap.times(c)
        ctx.ph.expect(len(times) >= 100, f"ch{c} produced records (n={len(times)})")
        if len(times) < 4:
            continue
        tstest.expect_cadence(ctx.ph, times, round(tstest.NS / hz), f"ch{c} gaps", edges="interior")
        tstest.expect_rate(ctx.ph, times, hz, f"ch{c}")


# --------------------------------------------------------------------
# Port-releasing phases: these reacquire the device themselves (the suite's port handle is closed
# before they run), because they exercise the real CLI as a subprocess or cold-reset the MCU.
# --------------------------------------------------------------------

TSCTL = os.path.join(os.path.dirname(__file__), "..", "util", "tsctl.py")

# STM32H523 unique-ID register base; the firmware builds its USB iSerialNumber (and thus the *IDN?
# serial) from the three 32-bit words here, prefixed with "LT4-".
TS_UID_BASE = 0x08FFF800
TS_SERIAL_PREFIX = "LT4-"


def _cli(port, *args, timeout=15):
    return subprocess.run(
        [sys.executable, TSCTL, "--port", port, *args],
        capture_output=True,
        text=True,
        timeout=timeout,
    )


@phase("tsctl cli", wires=None, releases_port=True)
def phase_tsctl_cli(ctx):
    """The shipped CLI tool, run exactly as a user would (subprocess per command): every subcommand
    round-trips, and `stream` emits only well-formed lines on the driven channel."""
    ph, port = ctx.ph, ctx.port

    r = _cli(port, "idn")
    ph.expect(
        r.returncode == 0 and r.stdout.startswith("Lectrobox,LectroTIC-4"),
        f"`tsctl idn` -> IDN ({r.stdout.strip()!r})",
    )
    ph.expect(_cli(port, "reset").returncode == 0, "`tsctl reset` ok")

    _cli(port, "slope", "0", "NEG")
    r = _cli(port, "slope", "0")
    ph.expect(r.stdout.strip() == "NEG", f"`tsctl slope 0` -> NEG ({r.stdout.strip()!r})")
    _cli(port, "slope", "0", "POS")

    _cli(port, "div", "0", "3")
    r = _cli(port, "div", "0")
    ph.expect(r.stdout.strip() == "3", f"`tsctl div 0` -> 3 ({r.stdout.strip()!r})")
    _cli(port, "div", "0", "1")

    _cli(port, "format", "BIN")
    r = _cli(port, "format")
    ph.expect(
        r.stdout.strip().upper().startswith("BIN"), f"`tsctl format` -> BIN ({r.stdout.strip()!r})"
    )
    _cli(port, "format", "TEXT")

    r = _cli(port, "raw", "*IDN?")
    ph.expect("LectroTIC-4" in r.stdout, f"`tsctl raw *IDN?` -> IDN ({r.stdout.strip()!r})")

    # save: runs the CONF:SAVE flash path without disturbing the live config; a final reset
    # leaves the device (and the persisted defaults) clean. True power-loss persistence is the
    # config-persist phase's job.
    _cli(port, "slope", "0", "NEG")
    _cli(port, "div", "0", "3")
    ph.expect(_cli(port, "save").returncode == 0, "`tsctl save` ok")
    r = _cli(port, "div", "0")
    ph.expect(r.stdout.strip() == "3", f"config undisturbed by save ({r.stdout.strip()!r})")
    ph.expect(_cli(port, "reset").returncode == 0, "`tsctl reset` restores defaults after save")
    r = _cli(port, "slope", "0")
    ph.expect(r.stdout.strip() == "POS", f"defaults back after reset ({r.stdout.strip()!r})")

    # stream: a real signal, run the CLI briefly, parse its stdout. -u so the child's stdout isn't
    # lost in a pipe buffer on kill.
    ctx.src.periodic(0, PULSE_HZ, PULSE_WIDTH_S)  # PG ch0 -> TS jack 0 (straight through)
    _cli(port, "slope", "0", "POS")
    proc = subprocess.Popen(
        [sys.executable, "-u", TSCTL, "--port", port, "stream"], stdout=subprocess.PIPE, text=True
    )
    time.sleep(2.5)
    proc.terminate()
    try:
        out, _ = proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, _ = proc.communicate()
    ctx.src.off()
    lines = [ln for ln in out.splitlines() if ln.strip()]
    line_re = re.compile(r"^\d+ \d+\.\d{9} [+-]$")
    good = [ln for ln in lines if line_re.match(ln)]
    print(f"  `tsctl stream`: {len(lines)} lines, {len(good)} well-formed")
    ph.expect(
        len(good) >= 5 and len(good) == len(lines), "all stream lines are '<ch> <sec>.<ns> <+|->'"
    )
    ph.expect(
        all(ln.split()[0] == "0" for ln in good), "all stream records are on the driven channel 0"
    )


def _reconnect(timeout_s=45):
    """After a BMP reset the LectroTIC-4's USB drops and re-enumerates (its /dev path can change,
    and the CDC ACM takes a few seconds to reappear and accept a connection), so autodetect afresh
    and retry until it answers *IDN?."""
    time.sleep(3.0)  # let the old USB device tear down before scanning
    deadline = time.monotonic() + timeout_s
    last = None
    while time.monotonic() < deadline:
        tic = None
        try:
            tic = LectroTIC4(None)
            tic.idn()
            return tic
        # autodetect_port() raises SystemExit (not Exception) when the device isn't enumerated
        # yet -- expected mid-reconnect, retry.
        except (Exception, SystemExit) as e:
            last = e
            if tic is not None:
                tic.close()
            time.sleep(1.0)
    raise RuntimeError(f"device did not re-enumerate within {timeout_s}s ({last})")


def _set_cfg(tic, cfg):
    for chx, (sl, dv) in enumerate(cfg):
        tic.set_slope(chx, sl)
        tic.set_divider(chx, int(dv))


def _read_cfg(tic):
    return [(tic.get_slope(c), tic.get_divider(c)) for c in tstest.CHANNELS]


def _persist_cfg(cfg):
    """Set cfg on all 4 channels, issue the explicit CONF:SAVE, then a query round-trip as a
    barrier: SCPI is processed in order, so the *IDN? reply only comes back after the
    (interrupt-masked) flash write has completed."""
    with LectroTIC4(None) as tic:
        _set_cfg(tic, cfg)
        tic.save()
        tic.idn()  # barrier: returns only once the save has finished


def bmp_on_this_timestamper(port):
    """(ok, reason): is a Black Magic Probe attached AND wired to the same timestamper this test
    talks to over USB? The config-persistence phases cold-reset the device THROUGH the BMP. If the
    probe is absent, or wired to some other target, bmpflash.reset() would not reset THIS
    timestamper -- the device would keep its RAM config and the phases would pass without testing
    anything. Guard against that vacuous pass by matching the probe target's SWD-read UID against
    the device's own *IDN? serial."""
    if not bmpflash.bmp_present():
        return False, "no Black Magic Probe attached"
    try:
        with LectroTIC4(port) as tic:
            idn_serial = tic.idn().split(",")[2]
    except Exception as e:
        return False, f"could not read timestamper *IDN?: {e}"
    try:
        bmp_serial = bmpflash.read_serial(TS_UID_BASE, TS_SERIAL_PREFIX)
    except RuntimeError as e:
        return False, str(e)
    if bmp_serial != idn_serial:
        return False, (
            f"BMP target UID is {bmp_serial}, not this timestamper's {idn_serial} -- "
            f"probe is on another board"
        )
    return True, ""


@phase("config persist", wires=None, releases_port=True, needs_bmp=True)
def phase_config_persist(ctx):
    """Channel config (slope+divider) must survive complete power loss. Set a non-default config,
    CONF:SAVE it, cold-reset the MCU through the BMP (clears SRAM, re-runs boot from flash), and
    confirm it came back. Every channel differs from the *RST defaults (POS, divider 1) so a blank
    or failed load can't masquerade as a pass."""
    ph = ctx.ph
    want = [("NEG", "3"), ("BOTH", "7"), ("POS", "5"), ("NEG", "9")]
    try:
        _persist_cfg(want)
        rc = bmpflash.reset()
        ph.expect(rc == 0, "BMP cold-reset issued (SRAM cleared)")
        tic = _reconnect()
    except Exception as e:
        ph.expect(False, f"config-persist setup/reset failed: {e}")
        return
    try:
        got = _read_cfg(tic)
        print(f"  expected {want}")
        print(f"  got      {got}")
        ph.expect(got == want, "all 4 channels' slope+divider restored from flash")
        # Leave device + flash at defaults (*RST persists defaults).
        tic.reset()
        tic.idn()
    finally:
        tic.close()


@phase("config torn", wires=None, releases_port=True, needs_bmp=True, iters=12)
def phase_config_persist_torn(ctx, iters):
    """Power-loss *during* a flash write must never destroy the last good config nor brick the
    device. A known non-default baseline A is persisted cleanly first (so one slot is valid). Each
    iteration then writes a *distinct* config B and cold-resets at a randomized delay spanning the
    coalesced erase/program window. The readback must be exactly A (write torn -> prior slot
    survived) or B (write completed) -- NEVER defaults or garbage and NEVER a no-show (the
    ECC-fault boot brick). A surviving B becomes the next baseline. (A brick here needs
    `bmpflash.py --erase` to recover.)"""
    ph = ctx.ph
    base = [("NEG", "3"), ("BOTH", "7"), ("POS", "5"), ("NEG", "9")]
    try:
        _persist_cfg(base)  # clean baseline write
        rc = bmpflash.reset()
        ph.expect(rc == 0, "baseline reset issued")
        tic = _reconnect()
        with tic:
            got = _read_cfg(tic)
        ph.expect(got == base, f"baseline A persisted cleanly {got}")
    except Exception as e:
        ph.expect(False, f"baseline setup failed: {e}")
        return

    for i in range(iters):
        # A distinct config each iteration so a completed write is unambiguously distinguishable
        # from the surviving baseline.
        cand = [("BOTH", str(i + 2)), ("NEG", str(i + 3)), ("POS", str(i + 4)), ("NEG", str(i + 5))]
        try:
            with LectroTIC4(None) as tic:
                _set_cfg(tic, cand)
                tic.save()  # no barrier: we WANT to reset mid-write
            # CONF:SAVE starts an interrupt-masked erase+program (tens of ms). Reset across that
            # window so some iterations interrupt it; the A/B slot + ECC must keep base intact.
            time.sleep(random.uniform(0.0, 0.06))
            rc = bmpflash.reset()
            if rc != 0:
                ph.expect(False, f"iter {i}: reset rc={rc}")
                continue
            tic = _reconnect()
        except Exception as e:
            ph.expect(False, f"iter {i}: DEVICE DID NOT COME BACK -- bricked by torn write ({e})")
            break
        with tic:
            got = _read_cfg(tic)
        if got == cand:
            ph.expect(True, f"iter {i}: write completed (B)")
            base = cand  # new surviving baseline
        else:
            ph.expect(got == base, f"iter {i}: write torn -> baseline A survived (got {got})")
    try:
        with LectroTIC4(None) as tic:
            tic.reset()
            tic.idn()  # barrier: *RST's default-persist finished
    except Exception:
        pass


# --------------------------------------------------------------------
# Runner
# --------------------------------------------------------------------


def main():
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--port", default=None, help="LectroTIC-4 serial port (default: autodetect)")
    p.add_argument("--pg-port", default=None, help="PG-4 serial port (default: autodetect)")
    p.add_argument(
        "--channel",
        choices=["0", "1", "2", "3", "all"],
        default="all",
        help="Input channel(s) for the per-channel phases (each drives the "
        "PG-4 output wired straight through to it); default: all four",
    )
    p.add_argument(
        "--duration", type=float, default=5.0, help="Capture window per steady phase, s (default 5)"
    )
    p.add_argument(
        "--phase",
        default=None,
        help="run only phases whose name contains this "
        "substring (case-insensitive); default: all",
    )
    p.add_argument(
        "--skip",
        default=None,
        help="skip phases whose name contains this substring (case-insensitive)",
    )
    args = p.parse_args()

    def want(name):
        if args.skip is not None and args.skip.lower() in name.lower():
            return False
        return args.phase is None or args.phase.lower() in name.lower()

    results = []

    def run(pd, name, ctx):
        print(f"\n=== {name} ===")
        ctx.ph = tstest.Phase()
        try:
            pd.fn(ctx, **pd.params)
        except PhaseSkip as skip:
            print(f"  SKIP: {skip}")
            results.append((name, None))
            return
        results.append((name, ctx.ph.ok))

    channels = [0, 1, 2, 3] if args.channel == "all" else [int(args.channel)]

    sg = Pulsegen(port=args.pg_port)
    src = tstest.Source(sg)
    print(f"Pulse source: {sg.idn()} (PG-4; base rate {PULSE_HZ:g} Hz)")

    with LectroTIC4(args.port) as tic:
        print(f"LectroTIC-4: {tic.port}; channels {channels}")
        port_path = tic.port
        print(f"Connected: {tic.idn()}")
        ctx = SimpleNamespace(
            tic=tic, src=src, channel=channels[0], duration=args.duration, wire=None, port=None
        )
        try:
            # Per-channel phases run for every selected channel, grouped by channel so a bad jack
            # reads as one contiguous block of failures.
            for ch in channels:
                ctx.channel = ch
                for wire in WIRES:
                    ctx.wire = wire
                    for pd in PHASES:
                        if (
                            pd.wires
                            and wire in pd.wires
                            and not pd.releases_port
                            and want(f"{pd.label} [ch{ch} {wire}]")
                        ):
                            run(pd, f"{pd.label} [ch{ch} {wire}]", ctx)
                ctx.wire = None
                for pd in PHASES:
                    if (
                        pd.wires is None
                        and pd.per_channel
                        and not pd.releases_port
                        and want(f"{pd.label} [ch{ch}]")
                    ):
                        run(pd, f"{pd.label} [ch{ch}]", ctx)
            ctx.wire = None
            ctx.channel = channels[0]
            for pd in PHASES:
                if (
                    pd.wires is None
                    and not pd.per_channel
                    and not pd.releases_port
                    and want(pd.label)
                ):
                    run(pd, pd.label, ctx)
        finally:
            tic.set_stream_enabled(False)
            src.off()

    # Port released: phases that reacquire the device themselves (CLI-under-test, BMP cold-reset
    # phases) run here. The BMP-needing phases skip -- never vacuously pass -- unless the probe is
    # wired to this exact unit.
    ctx.port = port_path
    try:
        bmp_ok, bmp_why = (None, None)
        for pd in PHASES:
            if not pd.releases_port or not want(pd.label):
                continue
            if pd.needs_bmp:
                if bmp_ok is None:
                    bmp_ok, bmp_why = bmp_on_this_timestamper(ctx.port)
                if not bmp_ok:
                    print(f"\n=== {pd.label}: SKIP ({bmp_why}) ===")
                    continue
            run(pd, pd.label, ctx)
    finally:
        src.off()
        sg.close()

    if not results:
        print(f"\nNo phase matched --phase {args.phase!r}")
        sys.exit(2)

    print("\n=== Summary ===")
    all_ok = True
    for name, ok in results:
        print(f"  {'SKIP' if ok is None else 'PASS' if ok else 'FAIL'}: {name}")
        all_ok &= ok is not False
    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
