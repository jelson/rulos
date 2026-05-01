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

The signal generator's output channel must be wired to the timestamper
input channel selected with --channel (default 1).

Usage:
  regression_test.py
  regression_test.py --channel 3
  regression_test.py --duration 10
  regression_test.py --siggen-host siggen2
"""

import argparse
import statistics
import sys
import time

sys.path.insert(0, __import__("os").path.dirname(__file__))
from tsctl import Timestamper
from siggen import Siggen


PULSE_HZ = 10
PULSE_WIDTH_S = 0.001  # siggen.periodic() hardcodes 1 ms
PERIOD_S = 1.0 / PULSE_HZ
TOLERANCE_S = 1e-6


def format_label(binary):
    return "BIN" if binary else "TEXT"


def settle(ts, duration_s=0.5):
    """Drain pending bytes after a config change so the next capture
    window starts on fresh data."""
    time.sleep(duration_s)
    ts.reset_input_buffer()


def capture(ts, channel, duration_s, binary):
    """Read records for duration_s, returning (timestamps for channel
    as floats, set of other channels seen, overflow comment count).
    Comments are only seen in text mode."""
    times = []
    other_chans = set()
    overflows = 0
    for rec in ts.read_for(duration_s, binary):
        if rec[0] == "comment":
            if "overflow" in rec[1].lower():
                overflows += 1
            continue
        _, ch, sec, ns = rec
        if ch == channel:
            times.append(sec + ns * 1e-9)
        else:
            other_chans.add(ch)
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


def phase_rising(ts, channel, duration_s, binary):
    print(f"\n=== Phase 1: rising edge only ({format_label(binary)}) ===")
    ts.set_slope(channel, "POS")
    ts.set_stream_enabled(True)
    settle(ts)

    times, others, overflows = capture(ts, channel, duration_s, binary)
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


def phase_divider(ts, channel, duration_s, binary, divider=5):
    print(f"\n=== Phase 3: rising edge, divider={divider} "
          f"({format_label(binary)}) ===")
    ts.set_slope(channel, "POS")
    ts.set_divider(channel, divider)
    ts.set_stream_enabled(True)
    settle(ts)

    times, others, overflows = capture(ts, channel, duration_s, binary)
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


def phase_both(ts, channel, duration_s, binary):
    print(f"\n=== Phase 2: both edges ({format_label(binary)}) ===")
    ts.set_slope(channel, "BOTH")
    ts.set_stream_enabled(True)
    settle(ts)

    times, others, overflows = capture(ts, channel, duration_s, binary)
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


def phase_output_gating(ts, channel, binary):
    print(f"\n=== Phase 4: OUTPut:STATe gating "
          f"({format_label(binary)}) ===")
    ts.set_slope(channel, "POS")
    ts.set_divider(channel, 1)

    ts.set_stream_enabled(False)
    ts.drain()
    times_off, _, _ = capture(ts, channel, 1.0, binary)

    ts.set_stream_enabled(True)
    settle(ts, 0.2)
    times_on, _, _ = capture(ts, channel, 1.0, binary)

    print(f"\n  with stream OFF: {len(times_off)} timestamps (expected 0)")
    print(f"  with stream ON:  {len(times_on)} timestamps "
          f"(expected ~{PULSE_HZ:.0f})")

    ok = True
    ok &= expect(len(times_off) == 0, "no timestamps arrive while OUTP:STAT OFF")
    ok &= expect(abs(len(times_on) - PULSE_HZ) <= 2,
                 f"~{PULSE_HZ:.0f} timestamps arrive after OUTP:STAT ON")
    return ok


def phase_scpi_errors(ts):
    print("\n=== Phase 5: SCPI error handling ===")
    ok = True

    ts.clear_errors()
    err = ts.get_error()
    ok &= expect(err.startswith("0,"),
                 f'SYST:ERR? after *CLS reports no-error (got: {err!r})')

    cases = [
        ("INP1:DIV 0",            "-222"),
        ("INP1:SLOP FROBNICATE",  "-104"),
        ("FOOBAR",                "-100"),
        ("INP1:DIV notanumber",   "-104"),
    ]
    for cmd, expected_code in cases:
        ts.clear_errors()
        ts.drain()
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

    for ch in (1, 2, 3, 4):
        ts.set_slope(ch, "BOTH")
        ts.set_divider(ch, 7)
    ts.set_stream_enabled(False)
    ts.drain()

    slope_before = ts.get_slope(channel)
    div_before   = ts.get_divider(channel)
    out_before   = ts.query("OUTP:STAT?")
    ok &= expect(slope_before == "BOTH",
                 f"pre-RST slope is BOTH (got: {slope_before!r})")
    ok &= expect(div_before == "7",
                 f"pre-RST divider is 7 (got: {div_before!r})")
    ok &= expect(out_before == "0",
                 f"pre-RST OUTP:STAT is 0 (got: {out_before!r})")

    ts.reset()
    ts.drain()

    for ch in (1, 2, 3, 4):
        slope = ts.get_slope(ch)
        div   = ts.get_divider(ch)
        ok &= expect(slope == "POS",
                     f"ch{ch} slope back to POS (got: {slope!r})")
        ok &= expect(div == "1",
                     f"ch{ch} divider back to 1 (got: {div!r})")
    out = ts.query("OUTP:STAT?")
    ok &= expect(out == "1", f"OUTP:STAT back to 1 (got: {out!r})")

    return ok


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default=None,
                   help="Timestamper serial port (default: autodetect)")
    p.add_argument("--siggen-host", default="siggen",
                   help="Signal generator hostname (default: siggen)")
    p.add_argument("--channel", type=int, choices=[1, 2, 3, 4], default=1,
                   help="Timestamper input channel under test (default: 1)")
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
            ts.set_stream_enabled(False)
            ts.drain()
            ts.reset()
            ts.drain()
            print(f"Connected to timestamper: {ts.idn()}")

            results = []
            for binary in (False, True):
                tag = format_label(binary)
                ts.set_format(binary)
                results.append((f"{tag} rising edge",
                                phase_rising(ts, args.channel, args.duration,
                                             binary)))
                results.append((f"{tag} both edges",
                                phase_both(ts, args.channel, args.duration,
                                           binary)))
                results.append((f"{tag} divider",
                                phase_divider(ts, args.channel, args.duration,
                                              binary)))
                results.append((f"{tag} output gating",
                                phase_output_gating(ts, args.channel, binary)))

            ts.set_format(False)
            results.append(("SCPI errors", phase_scpi_errors(ts)))
            results.append(("reset", phase_reset(ts, args.channel)))
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
