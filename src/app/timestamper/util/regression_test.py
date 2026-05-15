#!/usr/bin/env python3

"""LectroTIC-4 + Rigol DG1022Z end-to-end regression test.

Drives the signal generator to produce a known 10 Hz pulse train (1 ms
pulse width, 0..3.3 V) and verifies that the timestamper records it
correctly:

  Phase 1: rising-edge only (the *RST default)
    Expect ~1 timestamp per period, with ~100 ms gaps.

  Phase 2: both edges
    Expect 2 timestamps per period, alternating ~1 ms (rising→falling)
    and ~99 ms (falling→rising) gaps.

  Phase 3: rising edge with divider=5
    Expect 1 timestamp every 5 pulses, with ~500 ms gaps.

  Phase 4: OUTPut:STATe gating
    Turn streaming off, expect zero timestamps; turn back on, expect them
    to resume.

  Phase 5: SCPI error handling
    Send several deliberately-bad commands; verify each one latches the
    expected error code via SYST:ERR? and that the device keeps running.

  Phase 6: *RST
    Set a non-default state, send *RST, and verify every setting is back
    to its documented default.

  Phase 7: 10,000-pulse burst at 100 ns spacing
    Stress the DMA + ring-buffer path. Doubles as a hardware sanity
    check on newly-built devices: every pulse must come through with
    no overcapture or buffer-overflow comment.

  Phase 8: sustained 105 kHz throughput in BINARY mode
    Single-point throughput check: drive a steady 105 kHz pulse train
    for 5 s and verify every gap is within 1 µs of expected. Uses
    sustained_rate.test_rate, so passing here implies the device
    survives a representative ~95% of the measured 110 kHz USB ceiling.

The signal generator's output channel must be wired to the timestamper
input channel selected with --channel (default 0).

Usage:
  regression_test.py
  regression_test.py --channel 3
  regression_test.py --duration 10
  regression_test.py --siggen-host siggen2
"""

import argparse
import statistics
import sys

sys.path.insert(0, __import__("os").path.dirname(__file__))
from tsctl import Timestamper
from siggen import Siggen
import sustained_rate


PULSE_HZ = 10
PULSE_WIDTH_S = 0.001  # siggen.periodic() hardcodes 1 ms
PERIOD_S = 1.0 / PULSE_HZ
TOLERANCE_S = 1e-6


def format_label(ts):
    return "BIN" if ts.get_binary() else "TEXT"


def capture(ts, channel, duration_s):
    """Read records for duration_s, returning (timestamps for channel
    as floats, set of other channels seen, overflow comment count).
    Comments are only seen in text mode."""
    times = []
    other_chans = set()
    overflows = 0
    for r in ts.read_for(duration_s):
        if r.kind == "comment":
            if "overflow" in r.comment.lower():
                overflows += 1
            continue
        if r.channel == channel:
            times.append(r.time)
        else:
            other_chans.add(r.channel)
    return times, other_chans, overflows


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


def expect(condition, message):
    print(f"  [{'PASS' if condition else 'FAIL'}] {message}")
    return condition


def phase_rising(ts, channel, duration_s):
    print(f"\n=== Phase 1: rising edge only ({format_label(ts)}) ===")
    ts.set_slope(channel, "POS")
    ts.reset_input_buffer()

    times, others, overflows = capture(ts, channel, duration_s)
    expected = PULSE_HZ * duration_s
    print(f"\n  collected {len(times)} timestamps on ch{channel} "
          f"(expected ~{expected:.0f}); "
          f"other channels seen: {sorted(others) or 'none'}; "
          f"overflows: {overflows}")
    deltas = report_intervals("intervals", times)

    ok = True
    ok &= expect(abs(len(times) - expected) <= 2,
                 f"count within 2 of expected {expected:.0f}")
    ok &= expect(overflows == 0, "no buffer overflows")
    if deltas:
        ok &= expect(all(abs(d - PERIOD_S) < TOLERANCE_S for d in deltas),
                     f"all intervals within {TOLERANCE_S:.1e}s of "
                     f"{PERIOD_S * 1e3:.0f} ms")
    return ok


def phase_divider(ts, channel, duration_s, divider=5):
    print(f"\n=== Phase 3: rising edge, divider={divider} "
          f"({format_label(ts)}) ===")
    ts.set_slope(channel, "POS")
    ts.set_divider(channel, divider)
    ts.reset_input_buffer()

    times, others, overflows = capture(ts, channel, duration_s)
    expected = (PULSE_HZ / divider) * duration_s
    expected_period = PERIOD_S * divider
    print(f"\n  collected {len(times)} timestamps on ch{channel} "
          f"(expected ~{expected:.1f}); "
          f"other channels seen: {sorted(others) or 'none'}; "
          f"overflows: {overflows}")
    deltas = report_intervals("intervals", times)

    ok = True
    ok &= expect(abs(len(times) - expected) <= 1,
                 f"count within 1 of expected {expected:.1f}")
    ok &= expect(overflows == 0, "no buffer overflows")
    if deltas:
        ok &= expect(all(abs(d - expected_period) < TOLERANCE_S
                         for d in deltas),
                     f"all intervals within {TOLERANCE_S:.1e}s of "
                     f"{expected_period * 1e3:.0f} ms")

    # Restore divider so we don't leave the device in an unexpected state.
    ts.set_divider(channel, 1)
    return ok


def phase_both(ts, channel, duration_s):
    print(f"\n=== Phase 2: both edges ({format_label(ts)}) ===")
    ts.set_slope(channel, "BOTH")
    ts.reset_input_buffer()

    times, others, overflows = capture(ts, channel, duration_s)
    expected = 2 * PULSE_HZ * duration_s
    print(f"\n  collected {len(times)} timestamps on ch{channel} "
          f"(expected ~{expected:.0f}); "
          f"other channels seen: {sorted(others) or 'none'}; "
          f"overflows: {overflows}")

    if len(times) < 4:
        print("  too few samples, skipping interval analysis")
        return False

    deltas = [(times[i + 1] - times[i]) for i in range(len(times) - 1)]
    short = [d for d in deltas if d < PERIOD_S / 2]
    long = [d for d in deltas if d >= PERIOD_S / 2]
    print(f"  short deltas (rising→falling): n={len(short)} "
          f"median={statistics.median(short) * 1e3:.3f} ms")
    print(f"  long  deltas (falling→rising): n={len(long)} "
          f"median={statistics.median(long) * 1e3:.3f} ms")

    ok = True
    ok &= expect(abs(len(times) - expected) <= 4,
                 f"count within 4 of expected {expected:.0f}")
    ok &= expect(overflows == 0, "no buffer overflows")
    ok &= expect(abs(len(short) - len(long)) <= 1,
                 "short and long delta counts roughly equal")
    if short:
        ok &= expect(all(abs(d - PULSE_WIDTH_S) < TOLERANCE_S for d in short),
                     f"all short intervals within {TOLERANCE_S:.1e}s of "
                     f"{PULSE_WIDTH_S * 1e3:.0f} ms")
    if long:
        ok &= expect(all(abs(d - (PERIOD_S - PULSE_WIDTH_S)) < TOLERANCE_S
                         for d in long),
                     f"all long intervals within {TOLERANCE_S:.1e}s of "
                     f"{(PERIOD_S - PULSE_WIDTH_S) * 1e3:.0f} ms")
    return ok


def phase_output_gating(ts, channel):
    print(f"\n=== Phase 4: OUTPut:STATe gating "
          f"({format_label(ts)}) ===")
    ts.set_slope(channel, "POS")
    ts.set_divider(channel, 1)

    # Disable streaming and verify zero bytes flow. Use read_raw, not
    # capture(), because capture()->read_for would auto-enable streaming.
    ts.set_stream_enabled(False)
    ts.reset_input_buffer()
    bytes_off = ts.read_raw(1.0)

    # Re-enable streaming and drain the backlog of records the device
    # buffered while we were silent, so we count fresh ones in the
    # measurement window below.
    ts.set_stream_enabled(True)
    ts.reset_input_buffer()
    times_on, _, _ = capture(ts, channel, 1.0)

    print(f"\n  with stream OFF: {len(bytes_off)} bytes received "
          f"(expected 0)")
    print(f"  with stream ON:  {len(times_on)} timestamps "
          f"(expected ~{PULSE_HZ:.0f})")

    ok = True
    ok &= expect(len(bytes_off) == 0,
                 "no bytes arrive while OUTP:STAT OFF")
    ok &= expect(abs(len(times_on) - PULSE_HZ) <= 2,
                 f"~{PULSE_HZ:.0f} timestamps arrive after OUTP:STAT ON")
    return ok


def phase_scpi_errors(ts):
    print("\n=== Phase 5: SCPI error handling ===")
    ok = True

    # Silence the stream so query()'s auto-toggle doesn't fire; the
    # firmware clears the latched error on every successful command,
    # so an OUTP:STAT OFF/ON pair between the bad command and SYST:ERR?
    # would wipe the error we're trying to read.
    ts.set_stream_enabled(False)

    ts.clear_errors()
    err = ts.get_error()
    ok &= expect(err.startswith("0,"),
                 f'SYST:ERR? after *CLS reports no-error (got: {err!r})')

    cases = [
        ("INP0:DIV 0",            "-222"),
        ("INP0:SLOP FROBNICATE",  "-104"),
        ("FOOBAR",                "-100"),
        ("INP0:DIV notanumber",   "-104"),
    ]
    for cmd, expected_code in cases:
        ts.clear_errors()
        ts.reset_input_buffer()
        ts.send(cmd)
        err = ts.get_error()
        ok &= expect(expected_code in err,
                     f"`{cmd}` -> SYST:ERR? contains {expected_code} "
                     f"(got: {err!r})")

    err = ts.get_error()
    ok &= expect(err.startswith("0,"),
                 f"SYST:ERR? auto-clears after read (got: {err!r})")

    idn = ts.idn()
    ok &= expect(idn.startswith("Lectrobox,Timestamper"),
                 f"device still responds to *IDN? (got: {idn!r})")
    return ok


def phase_reset(ts, channel):
    print("\n=== Phase 6: *RST ===")
    ok = True

    for ch in range(4):
        ts.set_slope(ch, "BOTH")
        ts.set_divider(ch, 7)
    ts.set_stream_enabled(False)
    ts.reset_input_buffer()

    slope_before = ts.get_slope(channel)
    div_before   = ts.get_divider(channel)
    ok &= expect(slope_before == "BOTH",
                 f"pre-RST slope is BOTH (got: {slope_before!r})")
    ok &= expect(div_before == "7",
                 f"pre-RST divider is 7 (got: {div_before!r})")
    ok &= expect(ts.get_stream_enabled() is False,
                 "pre-RST stream is off")

    ts.reset()
    ts.reset_input_buffer()

    for ch in range(4):
        slope = ts.get_slope(ch)
        div   = ts.get_divider(ch)
        ok &= expect(slope == "POS",
                     f"ch{ch} slope back to POS (got: {slope!r})")
        ok &= expect(div == "1",
                     f"ch{ch} divider back to 1 (got: {div!r})")
    ok &= expect(ts.get_stream_enabled() is True,
                 "post-RST stream is on")

    return ok


def phase_burst(sg, ts, channel, ncyc=16383, spacing_ns=100):
    """Stress the on-device DMA + ring buffer: ask siggen for a burst
    of ncyc pulses spaced spacing_ns apart, repeating once per second,
    and verify every pulse comes through with no overcapture/overflow.
    Doubles as a hardware sanity check for newly-built devices."""
    print(f"\n=== Phase 7: {ncyc}-pulse burst at {spacing_ns} ns ===")

    ts.set_slope(channel, "POS")
    ts.set_divider(channel, 1)
    # TEXT mode so the firmware's # overflow / # overcapture comments
    # are visible if anything goes wrong.
    ts.set_binary(False)

    actual_ns = sg.pulse_burst(spacing_ns, ncyc=ncyc)
    ts.discard_pending()

    # Capture for a bit longer than 2 burst periods so we see at least
    # two full bursts even if the first one is in flight when we start.
    times = []
    overcaptures = 0
    overflows = 0
    other_chans = set()
    for r in ts.read_for(3.5):
        if r.kind == "comment":
            text = r.comment.lower()
            if "overcapture" in text:
                overcaptures += 1
            if "overflow" in text:
                overflows += 1
            print(f"    | {r.comment}")
            continue
        if r.channel == channel:
            times.append(r.time)
        else:
            other_chans.add(r.channel)

    # Group into bursts: timestamps within 0.5 s of each other are
    # the same burst (siggen repeats once per second).
    bursts = []
    i = 0
    while i < len(times):
        size = 1
        t0 = times[i]
        j = i + 1
        while j < len(times) and abs(times[j] - t0) < 0.5:
            size += 1
            j += 1
        bursts.append(size)
        i = j

    complete = [s for s in bursts if s == ncyc]
    print(f"\n  actual spacing: {actual_ns:.1f} ns (requested {spacing_ns})")
    print(f"  bursts captured: {bursts}")
    print(f"  channels seen: {sorted(other_chans | {channel})}")
    print(f"  overcaptures: {overcaptures}, overflows: {overflows}")

    ok = True
    ok &= expect(len(complete) >= 2,
                 f"at least 2 complete bursts of {ncyc} pulses captured")
    ok &= expect(overcaptures == 0, "no DMA overcaptures")
    ok &= expect(overflows == 0, "no ring-buffer overflows")
    ok &= expect(not other_chans,
                 f"no spurious activity on other channels (got {other_chans})")
    return ok


def phase_sustained_rate(sg, ts, channel, hz=105000):
    """Single-point throughput check: drive a steady hz pulse train
    and verify every gap is within 1 µs of the expected period. Uses
    sustained_rate.test_rate so this stays a thin wrapper around the
    same code the dedicated binary-search tool uses."""
    print(f"\n=== Phase 8: sustained rate @ {hz} Hz (BIN) ===")
    ts.set_slope(channel, "POS")
    ts.set_divider(channel, 1)
    ts.set_binary(True)
    ok = sustained_rate.test_rate(
        sg, ts, hz,
        settle_s=2.0, measure_s=5.0, tolerance=1e-6, verbose=False)
    return ok


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default=None,
                   help="Timestamper serial port (default: autodetect)")
    p.add_argument("--siggen-host", default="siggen",
                   help="Signal generator hostname (default: siggen)")
    p.add_argument("--channel", type=int, choices=[0, 1, 2, 3], default=0,
                   help="Timestamper input channel under test (default: 0)")
    p.add_argument("--duration", type=float, default=5.0,
                   help="Capture window per phase, seconds (default: 5)")
    args = p.parse_args()

    with Timestamper(args.port) as ts:
        print(f"Timestamper: {ts.port}")
        print(f"Signal generator: {args.siggen_host}")
        print(f"Channel under test: {args.channel}")
        print(f"Capture duration per phase: {args.duration} s")

        sg = Siggen(host=args.siggen_host)
        print(f"Connected to signal generator: {sg.idn()}")
        print(f"\nConfiguring siggen for {PULSE_HZ} Hz, "
              f"{PULSE_WIDTH_S * 1e3:.0f} ms pulse width...")
        sg.periodic(PULSE_HZ)

        try:
            # *RST clears the device's timestamp ring as well as
            # channel config, so any leftover records from a prior
            # aborted run are dropped here.
            ts.reset()
            ts.discard_pending()
            print(f"Connected to timestamper: {ts.idn()}")

            results = []
            for binary in (False, True):
                ts.set_binary(binary)
                tag = format_label(ts)
                results.append((f"{tag} rising edge",
                                phase_rising(ts, args.channel, args.duration)))
                results.append((f"{tag} both edges",
                                phase_both(ts, args.channel, args.duration)))
                results.append((f"{tag} divider",
                                phase_divider(ts, args.channel,
                                              args.duration)))
                results.append((f"{tag} output gating",
                                phase_output_gating(ts, args.channel)))

            ts.set_binary(False)
            results.append(("SCPI errors", phase_scpi_errors(ts)))
            results.append(("reset", phase_reset(ts, args.channel)))
            results.append(("burst", phase_burst(sg, ts, args.channel)))
            results.append(("sustained rate",
                            phase_sustained_rate(sg, ts, args.channel)))
        finally:
            ts.set_stream_enabled(False)
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
