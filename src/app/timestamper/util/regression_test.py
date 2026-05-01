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

import serial

# Re-use tsctl helpers (autodetect, raw send/query, drain).
sys.path.insert(0, __import__("os").path.dirname(__file__))
import tsctl
from siggen import Siggen


PULSE_HZ = 10
PULSE_WIDTH_S = 0.001  # siggen.periodic() hardcodes 1 ms
PERIOD_S = 1.0 / PULSE_HZ
TOLERANCE_S = 1e-6


def capture(ser, channel, duration_s):
    """Read timestamp lines for the given channel for duration_s, returning
    a list of timestamps as floats (seconds since boot)."""
    ts = []
    other_chans = set()
    overflows = 0
    deadline = time.monotonic() + duration_s
    ser.timeout = 0.2
    while time.monotonic() < deadline:
        line = ser.readline().decode(errors="replace").strip()
        if not line:
            continue
        if line.startswith("#"):
            if "overflow" in line.lower():
                overflows += 1
            continue
        parts = line.split()
        if len(parts) != 2:
            continue
        try:
            ch = int(parts[0])
            t = float(parts[1])
        except ValueError:
            continue
        if ch == channel:
            ts.append(t)
        else:
            other_chans.add(ch)
    return ts, other_chans, overflows


def report_intervals(label, ts):
    if len(ts) < 2:
        print(f"  {label}: <2 samples, can't compute intervals")
        return None
    deltas = [(ts[i + 1] - ts[i]) for i in range(len(ts) - 1)]
    print(f"  {label}: count={len(deltas)} "
          f"min={min(deltas) * 1e3:.3f} ms "
          f"median={statistics.median(deltas) * 1e3:.3f} ms "
          f"max={max(deltas) * 1e3:.3f} ms "
          f"stdev={statistics.stdev(deltas) * 1e6:.1f} µs")
    return deltas


def expect(condition, message):
    print(f"  [{'PASS' if condition else 'FAIL'}] {message}")
    return condition


def phase_rising(ser, channel, duration_s):
    print("\n=== Phase 1: rising edge only ===")
    tsctl.send(ser, f"INP{channel}:SLOP POS")
    tsctl.send(ser, "OUTP:STAT ON")
    ser.reset_input_buffer()
    time.sleep(0.5)  # let the device emit a fresh # boundary line, then steady stream
    ser.reset_input_buffer()

    ts, others, overflows = capture(ser, channel, duration_s)
    expected = PULSE_HZ * duration_s
    print(f"\n  collected {len(ts)} timestamps on ch{channel} "
          f"(expected ~{expected:.0f}); "
          f"other channels seen: {sorted(others) or 'none'}; "
          f"overflows: {overflows}")
    deltas = report_intervals("intervals", ts)

    ok = True
    ok &= expect(abs(len(ts) - expected) <= 2,
                 f"count within 2 of expected {expected:.0f}")
    ok &= expect(overflows == 0, "no buffer overflows")
    if deltas:
        ok &= expect(all(abs(d - PERIOD_S) < TOLERANCE_S for d in deltas),
                     f"all intervals within {TOLERANCE_S:.1e}s of "
                     f"{PERIOD_S * 1e3:.0f} ms")
    return ok


def phase_divider(ser, channel, duration_s, divider=5):
    print(f"\n=== Phase 3: rising edge, divider={divider} ===")
    tsctl.send(ser, f"INP{channel}:SLOP POS")
    tsctl.send(ser, f"INP{channel}:DIV {divider}")
    tsctl.send(ser, "OUTP:STAT ON")
    ser.reset_input_buffer()
    time.sleep(0.5)
    ser.reset_input_buffer()

    ts, others, overflows = capture(ser, channel, duration_s)
    expected = (PULSE_HZ / divider) * duration_s
    expected_period = PERIOD_S * divider
    print(f"\n  collected {len(ts)} timestamps on ch{channel} "
          f"(expected ~{expected:.1f}); "
          f"other channels seen: {sorted(others) or 'none'}; "
          f"overflows: {overflows}")
    deltas = report_intervals("intervals", ts)

    ok = True
    ok &= expect(abs(len(ts) - expected) <= 1,
                 f"count within 1 of expected {expected:.1f}")
    ok &= expect(overflows == 0, "no buffer overflows")
    if deltas:
        ok &= expect(all(abs(d - expected_period) < TOLERANCE_S
                         for d in deltas),
                     f"all intervals within {TOLERANCE_S:.1e}s of "
                     f"{expected_period * 1e3:.0f} ms")

    # Restore divider so we don't leave the device in an unexpected state.
    tsctl.send(ser, f"INP{channel}:DIV 1")
    return ok


def phase_both(ser, channel, duration_s):
    print("\n=== Phase 2: both edges ===")
    tsctl.send(ser, f"INP{channel}:SLOP BOTH")
    tsctl.send(ser, "OUTP:STAT ON")
    ser.reset_input_buffer()
    time.sleep(0.5)
    ser.reset_input_buffer()

    ts, others, overflows = capture(ser, channel, duration_s)
    expected = 2 * PULSE_HZ * duration_s
    print(f"\n  collected {len(ts)} timestamps on ch{channel} "
          f"(expected ~{expected:.0f}); "
          f"other channels seen: {sorted(others) or 'none'}; "
          f"overflows: {overflows}")

    if len(ts) < 4:
        print("  too few samples, skipping interval analysis")
        return False

    deltas = [(ts[i + 1] - ts[i]) for i in range(len(ts) - 1)]
    # Two interleaved populations: short (rising→falling, ~pulse_width)
    # and long (falling→rising, ~period - pulse_width). Sort by index
    # parity *after* identifying which delta is which from the data.
    short = [d for d in deltas if d < PERIOD_S / 2]
    long = [d for d in deltas if d >= PERIOD_S / 2]
    print(f"  short deltas (rising→falling): n={len(short)} "
          f"median={statistics.median(short) * 1e3:.3f} ms")
    print(f"  long  deltas (falling→rising): n={len(long)} "
          f"median={statistics.median(long) * 1e3:.3f} ms")

    ok = True
    ok &= expect(abs(len(ts) - expected) <= 4,
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


def phase_output_gating(ser, channel):
    print("\n=== Phase 4: OUTPut:STATe gating ===")
    # Re-establish a known starting state.
    tsctl.send(ser, f"INP{channel}:SLOP POS")
    tsctl.send(ser, f"INP{channel}:DIV 1")

    tsctl.send(ser, "OUTP:STAT OFF")
    tsctl.drain(ser)
    ts_off, _, _ = capture(ser, channel, 1.0)

    tsctl.send(ser, "OUTP:STAT ON")
    ser.reset_input_buffer()
    time.sleep(0.2)
    ser.reset_input_buffer()
    ts_on, _, _ = capture(ser, channel, 1.0)

    print(f"\n  with stream OFF: {len(ts_off)} timestamps (expected 0)")
    print(f"  with stream ON:  {len(ts_on)} timestamps "
          f"(expected ~{PULSE_HZ:.0f})")

    ok = True
    ok &= expect(len(ts_off) == 0, "no timestamps arrive while OUTP:STAT OFF")
    ok &= expect(abs(len(ts_on) - PULSE_HZ) <= 2,
                 f"~{PULSE_HZ:.0f} timestamps arrive after OUTP:STAT ON")
    return ok


def phase_scpi_errors(ser):
    print("\n=== Phase 5: SCPI error handling ===")
    ok = True

    # Make sure we start clean.
    tsctl.send(ser, "*CLS")
    err = tsctl.query(ser, "SYST:ERR?")
    ok &= expect(err.startswith("0,"),
                 f'SYST:ERR? after *CLS reports no-error (got: {err!r})')

    # Each entry: (command, substring expected in the latched error).
    cases = [
        ("INP1:DIV 0",            "-222"),  # Divider must be >= 1
        ("INP1:SLOP FROBNICATE",  "-104"),  # Bad slope
        ("FOOBAR",                "-100"),  # Unknown command
        ("INP1:DIV notanumber",   "-104"),  # Bad number
    ]
    for cmd, expected_code in cases:
        tsctl.send(ser, "*CLS")
        tsctl.drain(ser)
        tsctl.send(ser, cmd)
        err = tsctl.query(ser, "SYST:ERR?")
        ok &= expect(expected_code in err,
                     f"`{cmd}` -> SYST:ERR? contains {expected_code} "
                     f"(got: {err!r})")

    # SYST:ERR? clears the latched error -- next read should be clean.
    err = tsctl.query(ser, "SYST:ERR?")
    ok &= expect(err.startswith("0,"),
                 f"SYST:ERR? auto-clears after read (got: {err!r})")

    # Device still responsive after a salvo of bad commands.
    idn = tsctl.query(ser, "*IDN?")
    ok &= expect(idn.startswith("Lectrobox,Timestamper"),
                 f"device still responds to *IDN? (got: {idn!r})")
    return ok


def phase_reset(ser, channel):
    print("\n=== Phase 6: *RST ===")
    ok = True

    # Drive the device into a non-default state on every channel.
    for ch in (1, 2, 3, 4):
        tsctl.send(ser, f"INP{ch}:SLOP BOTH")
        tsctl.send(ser, f"INP{ch}:DIV 7")
    tsctl.send(ser, "OUTP:STAT OFF")
    tsctl.drain(ser)

    # Sanity check that the non-default state actually took.
    slope_before = tsctl.query(ser, f"INP{channel}:SLOP?")
    div_before   = tsctl.query(ser, f"INP{channel}:DIV?")
    out_before   = tsctl.query(ser, "OUTP:STAT?")
    ok &= expect(slope_before == "BOTH",
                 f"pre-RST slope is BOTH (got: {slope_before!r})")
    ok &= expect(div_before == "7",
                 f"pre-RST divider is 7 (got: {div_before!r})")
    ok &= expect(out_before == "0",
                 f"pre-RST OUTP:STAT is 0 (got: {out_before!r})")

    tsctl.send(ser, "*RST")
    tsctl.drain(ser)

    # Every channel should be back to rising / divider=1, streaming on.
    for ch in (1, 2, 3, 4):
        slope = tsctl.query(ser, f"INP{ch}:SLOP?")
        div   = tsctl.query(ser, f"INP{ch}:DIV?")
        ok &= expect(slope == "POS",
                     f"ch{ch} slope back to POS (got: {slope!r})")
        ok &= expect(div == "1",
                     f"ch{ch} divider back to 1 (got: {div!r})")
    out = tsctl.query(ser, "OUTP:STAT?")
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

    port = args.port or tsctl.autodetect_port()
    print(f"Timestamper: {port}")
    print(f"Signal generator: {args.siggen_host}")
    print(f"Channel under test: {args.channel}")
    print(f"Capture duration per phase: {args.duration} s")

    sg = Siggen(host=args.siggen_host)
    print(f"Connected to signal generator: {sg.idn()}")
    print(f"\nConfiguring siggen for {PULSE_HZ} Hz, "
          f"{PULSE_WIDTH_S * 1e3:.0f} ms pulse width...")
    sg.periodic(PULSE_HZ)

    ser = serial.Serial(port, timeout=0.5)
    try:
        tsctl.send(ser, "OUTP:STAT OFF")
        tsctl.drain(ser)
        tsctl.send(ser, "*RST")
        tsctl.drain(ser)
        idn = tsctl.query(ser, "*IDN?")
        print(f"Connected to timestamper: {idn}")

        results = []
        results.append(("rising edge", phase_rising(ser, args.channel,
                                                    args.duration)))
        results.append(("both edges", phase_both(ser, args.channel,
                                                 args.duration)))
        results.append(("divider", phase_divider(ser, args.channel,
                                                 args.duration)))
        results.append(("output gating", phase_output_gating(ser,
                                                             args.channel)))
        results.append(("SCPI errors", phase_scpi_errors(ser)))
        results.append(("reset", phase_reset(ser, args.channel)))
    finally:
        tsctl.send(ser, "OUTP:STAT OFF")
        ser.close()
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
