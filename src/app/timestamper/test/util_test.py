#!/usr/bin/env python3

"""End-to-end tests for the demo utilities in ../util, driven by a PG-4.

Each utility runs as a real subprocess (exactly the way a user runs it) with --updates N, and its
stdout is parsed and scored strictly: PG-4 frequencies on the 250 ps grid must read back exactly
(within measurement resolution), regime flags must appear precisely when the regime calls for
them, and a reading that should be clean may not carry OVERRUN. The frequency counter is swept
from 1 Hz (a software-scheduled burst envelope) through eight exact continuous points up to
20 MHz (in range thanks to the divider's hardware-prescaler share) and silence -- so every
auto-ranging path is exercised, plus a simultaneous four-channel case with two distinct
frequencies.

Usage:
  util_test.py                     run everything
  util_test.py --only freq         run tests whose name contains "freq"
  util_test.py --pg-port /dev/ttyACM2
"""

import argparse
import math
import os
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "util"))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "pulsegen", "util"))

import allan
import pgctl
import tsctl
import tstest

UTIL_DIR = os.path.join(os.path.dirname(__file__), "..", "util")

UNIT_HZ = {"Hz": 1.0, "kHz": 1e3, "MHz": 1e6}

# Continuous PG-4 frequencies land exactly on its 250 ps period grid (verified to ~20 ppm
# elsewhere); 200 ppm catches a wrong reading while clearing quantization and display rounding.
REL_TOL = 200e-6

FAILURES = []


def check(cond, msg):
    print(f"    [{'ok' if cond else 'FAIL'}] {msg}")
    if not cond:
        FAILURES.append(msg)


def run_util(name, *args, timeout_s=60):
    """Run one utility as a subprocess and return (returncode, lines): the utilities' live
    status() display overwrites with \\r and only finalized readings get a newline, so each
    stdout line's text is whatever followed its last carriage return."""
    cmd = [sys.executable, os.path.join(UTIL_DIR, name)] + [str(a) for a in args]
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout_s)
    if proc.stderr:
        print(f"    | {name} stderr: {proc.stderr.strip()}")
    return proc.returncode, [
        line.split("\r")[-1].strip() for line in proc.stdout.splitlines() if line.strip("\r ")
    ]


def parse_freq(text, ch=0):
    """One channel's 'value unit' from a reading block, in Hz, or None for a signalless
    channel."""
    m = re.search(rf"ch{ch}\s+([0-9.]+) (MHz|kHz|Hz)\b", text)
    return float(m.group(1)) * UNIT_HZ[m.group(2)] if m else None


def reading_blocks(lines):
    """Group freq_counter's stdout into its 5-line reading blocks (ch0..ch3 plus the gate
    footer), each joined into one searchable string."""
    return ["  ".join(lines[i : i + 5]) for i, line in enumerate(lines) if line.startswith("ch0")]


def test_freq_counter(src):
    # Five exact continuous decades: each invocation cold-starts, so every run exercises the
    # range-down acquisition before its readings.
    # The top three points sit at and above the drain's raw ceiling (~10 M edges/s); they read
    # exactly because the divider's hardware-prescaler share keeps the capture load at 1/8th.
    for hz in (2_000, 10_000, 100_000, 1_000_000, 5_000_000, 10_000_000, 16_000_000, 20_000_000):
        src.periodic(0, hz, 0.25 / hz)
        rc, lines = run_util("freq_counter.py", "--updates", 2)
        readings = [(b, parse_freq(b)) for b in reading_blocks(lines) if parse_freq(b) is not None]
        check(rc == 0 and len(readings) == 2, f"{hz} Hz: two readings ({len(readings)})")
        for block, f in readings:
            check(abs(f - hz) <= hz * REL_TOL, f"{hz} Hz: reads {f} (within {REL_TOL * 1e6} ppm)")
            check("OVERRUN" not in block, f"{hz} Hz: no loss reported")

    # 1 Hz: a single-pulse burst envelope. The PG-4's repetition is software-scheduled at the
    # requested 1/s plus a ~10 ms tick, so the counter must report that ACTUAL interval --
    # slightly under 1 Hz -- via its stretched-gate slow path.
    src.burst(0, 1000, 1)
    rc, lines = run_util("freq_counter.py", "--updates", 1, timeout_s=90)
    readings = [parse_freq(b) for b in reading_blocks(lines) if parse_freq(b) is not None]
    check(rc == 0 and len(readings) == 1, f"1 Hz: one reading ({len(readings)})")
    if readings:
        check(0.95 <= readings[0] <= 1.005, f"1 Hz envelope: reads {readings[0]}")

    # Silence: the gate must stretch to its maximum, then every channel must say so.
    src.off()
    rc, lines = run_util("freq_counter.py", "--updates", 1, timeout_s=90)
    blocks = reading_blocks(lines)
    check(
        rc == 0 and blocks and blocks[-1].count("no signal") == 4,
        f"silence reported as 'no signal' on all four channels "
        f"({blocks[-1] if blocks else 'no blocks'})",
    )


def test_freq_counter_multichannel(src):
    # The PG-4's two ASYNC period domains drive all four inputs at once: ch0/ch1 at 50 kHz,
    # ch2/ch3 at 2 kHz. Every reading line must show all four channels reading their own exact
    # frequency simultaneously, each independently auto-ranged within its share of the budget.
    src.async_rates(50_000, 2_000)
    rc, lines = run_util("freq_counter.py", "--updates", 3, timeout_s=90)
    readings = [b for b in reading_blocks(lines) if parse_freq(b) is not None]
    check(rc == 0 and len(readings) == 3, f"multichannel: three readings ({len(readings)})")
    for block in readings:
        for ch, hz in ((0, 50_000), (1, 50_000), (2, 2_000), (3, 2_000)):
            f = parse_freq(block, ch)
            check(
                f is not None and abs(f - hz) <= hz * REL_TOL,
                f"multichannel: ch{ch} reads {f} ({hz} expected)",
            )
        check("OVERRUN" not in block, "multichannel: no loss reported")


def test_phase_comparator(src):
    # Locked equal frequencies: 5 kHz on all four sync channels, channel 1 delayed +100 ns. The
    # comparator must read both frequencies exactly, ratio 1, a +100 ns offset, and ~zero drift.
    src.sync(delays_ns=[0, 100, 0, 0], period_ns=200_000)
    rc, lines = run_util("phase_comparator.py", "--updates", 5, timeout_s=90)
    readings = [line for line in lines if line.startswith("A ")]
    check(rc == 0 and len(readings) == 5, f"locked: five readings ({len(readings)})")
    drifts = []
    for line in readings:
        m = re.match(
            r"A\s+([0-9.]+) (\w+)\s+B\s+([0-9.]+) (\w+)\s+B/A ([0-9.]+)\s+dphi\s+([+-][0-9.]+) ns"
            r"(?:\s+drift\s+([+-][0-9.]+) ns/s)?",
            line,
        )
        check(m is not None, f"locked: parsable reading ({line})")
        if not m:
            continue
        fa = float(m.group(1)) * UNIT_HZ[m.group(2)]
        fb = float(m.group(3)) * UNIT_HZ[m.group(4)]
        check(abs(fa - 5000) <= 5000 * REL_TOL, f"locked: A reads {fa}")
        check(abs(fb - 5000) <= 5000 * REL_TOL, f"locked: B reads {fb}")
        check(abs(float(m.group(5)) - 1.0) <= 1e-6, f"locked: ratio {m.group(5)}")
        check(abs(float(m.group(6)) - 100) <= 10, f"locked: dphi {m.group(6)} ns (+100 expected)")
        if m.group(7) is not None:
            drifts.append(float(m.group(7)))
    check(drifts and all(abs(d) <= 2 for d in drifts), f"locked: drift ~0 ns/s ({drifts})")

    # Exact 4:1 ratio across the PG-4's two ASYNC period domains (ch0 vs ch2). ASYNC mode also
    # runs ch1/ch3, which the comparator doesn't manage, so park their dividers high first to
    # keep the wire in regime.
    src.async_rates(40_000, 10_000)
    with tsctl.LectroTIC4() as tic:
        for ch in (1, 3):
            tic.set_divider(ch, 1_000_000)
    rc, lines = run_util("phase_comparator.py", "--channels", 0, 2, "--updates", 3, timeout_s=90)
    readings = [line for line in lines if line.startswith("A ")]
    check(rc == 0 and len(readings) == 3, f"ratio: three readings ({len(readings)})")
    for line in readings:
        m = re.search(r"B/A ([0-9.]+)", line)
        check(
            m is not None and abs(float(m.group(1)) - 0.25) <= 5e-7,
            f"ratio: B/A is exactly 0.25 ({line})",
        )
        check("dphi" not in line, "ratio: no phase readout for unequal frequencies")
    with tsctl.LectroTIC4() as tic:
        tstest.configure(tic)


def test_reader_error_propagation():
    # Hardware-free: feed tsctl's streaming path from a fake serial port that produces two valid
    # binary records and then dies. The reader thread's error must re-raise in the consumer (a
    # silently truncated capture could pass for a complete measurement); the records that arrived
    # before the fault must still be delivered.
    def record(channel, seconds, tick, rising=True):
        counter = (channel << 30) | ((1 << 29) if rising else 0) | tick
        return struct.pack("<II", seconds, counter)

    class FailingPort:
        timeout = None

        def __init__(self):
            self.reads = 0

        def read(self, n):
            self.reads += 1
            if self.reads == 1:
                return record(0, 1, 100) + record(0, 1, 200)
            raise IOError("port fell off")

    tic = tsctl.LectroTIC4.__new__(tsctl.LectroTIC4)
    tic._ser = FailingPort()
    records = []
    try:
        for r in tic._stream_records(time.monotonic() + 5.0):
            records.append(r)
        check(False, "reader error propagated (stream ended silently instead)")
    except IOError as e:
        check(str(e) == "port fell off", f"reader error propagated as-is ({e})")
    check(
        len(records) == 2 and all(isinstance(r, tsctl.Timestamp) for r in records),
        f"records before the fault were still delivered ({records})",
    )


def test_allan_math():
    # Deterministic closed form: an alternating time-error series +a,-a,+a,... has second
    # difference 4a at every odd m and 0 at every even m, so ADEV(m=1) = a*sqrt(8)/tau0 exactly.
    a, tau0 = 100.0, 1000.0
    x = [a if i % 2 == 0 else -a for i in range(1000)]
    table = allan.adev_table(x, tau0)
    check(len(table) >= 6, f"ladder produced {len(table)} taus")
    check(abs(table[0][0] - tau0 / tsctl.NS) < 1e-15, f"first tau is tau0 ({table[0][0]})")
    expected = a * math.sqrt(8) / tau0
    check(abs(table[0][1] - expected) <= expected * 1e-12, f"ADEV(m=1) exact ({table[0][1]})")
    check(table[1][1] == 0.0, f"ADEV(m=2) exactly zero ({table[1][1]})")


def test_allan_hardware(src):
    # 100 kHz for 8 s: the divider auto-ranges to ~1 kHz of phase samples and the run must
    # complete loss-free with a sane stability table (PG-4 crystal against the lab reference).
    src.periodic(0, 100_000, 2.5e-6)
    rc, lines = run_util("allan.py", "--duration", 8, timeout_s=90)
    check(rc == 0, f"allan exits 0 (rc={rc}; {lines[-1] if lines else 'no output'})")
    rows = []
    for line in lines:
        m = re.match(r"[0-9.]+ (?:ns|us|ms|s)\s+([0-9.]+e[+-][0-9]+)\s+[0-9]+$", line)
        if m:
            rows.append(float(m.group(1)))
    check(len(rows) >= 6, f"stability table spans the ladder ({len(rows)} taus)")
    # Exactly 0 is legitimate: a source phase-locked to the instrument's own reference can sit
    # below the 4 ns quantization floor for the whole run, leaving every gap bit-identical.
    check(rows and all(0 <= v < 1e-5 for v in rows), f"ADEV values sane ({rows[:3]}...)")
    check(
        any("mean frequency 100.00" in line for line in lines),
        "mean frequency reported at 100 kHz",
    )


def test_pulse_width(src):
    # 2 kHz, 100 us wide: 20% duty. Width is hardware-exact on both instruments; 1 us of slack
    # allows for input-stage threshold asymmetry without letting a wrong pairing through.
    for hz, width_us, duty in ((2_000, 100, 20.0), (5_000, 100, 50.0)):
        src.periodic(0, hz, width_us * 1e-6)
        rc, lines = run_util("pulse_width.py", "--updates", 3, timeout_s=90)
        readings = [line for line in lines if line.startswith("width ")]
        check(rc == 0 and len(readings) == 3, f"{hz} Hz: three readings ({len(readings)})")
        for line in readings:
            m = re.match(
                r"width\s+([0-9.]+) (ns|us|ms|s)\s+duty\s+([0-9.]+) %\s+"
                r"rate\s+([0-9.]+) (MHz|kHz|Hz)",
                line,
            )
            check(m is not None, f"{hz} Hz: parsable reading ({line})")
            if not m:
                continue
            w_us = float(m.group(1)) * {"ns": 1e-3, "us": 1.0, "ms": 1e3, "s": 1e6}[m.group(2)]
            check(abs(w_us - width_us) <= 1.0, f"{hz} Hz: width {w_us} us ({width_us} expected)")
            check(abs(float(m.group(3)) - duty) <= 0.5, f"{hz} Hz: duty {m.group(3)} %")
            rate = float(m.group(4)) * UNIT_HZ[m.group(5)]
            check(abs(rate - hz) <= hz * REL_TOL, f"{hz} Hz: rate reads {rate}")
            check("OVERRUN" not in line, f"{hz} Hz: no loss reported")

    # Width mode needs every edge (no divider), so it must refuse rates whose edges would blow
    # the gate budget rather than run the instrument out of regime.
    src.periodic(0, 50_000, 5e-6)
    rc, lines = run_util("pulse_width.py", "--updates", 1, timeout_s=60)
    check(
        rc == 0 and any(line.startswith("too fast") for line in lines),
        f"50 kHz refused as too fast ({lines[-1] if lines else 'no output'})",
    )


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pg-port", default=None, help="PG-4 serial port (default: autodetect)")
    p.add_argument("--only", default="", help="run only tests whose name contains this substring")
    args = p.parse_args()

    tests = [
        ("freq_counter", test_freq_counter),
        ("freq_counter_multichannel", test_freq_counter_multichannel),
        ("phase_comparator", test_phase_comparator),
        ("reader_error", lambda src: test_reader_error_propagation()),
        ("allan_math", lambda src: test_allan_math()),
        ("allan_hardware", test_allan_hardware),
        ("pulse_width", test_pulse_width),
    ]
    tests = [(n, fn) for n, fn in tests if args.only in n]

    # The bench comes up only if a selected test needs it, so the hardware-free tests run
    # anywhere.
    HARDWARE_FREE = {"reader_error", "allan_math"}
    needs_bench = any(n not in HARDWARE_FREE for n, _ in tests)
    src = tstest.Source(pgctl.Pulsegen(args.pg_port)) if needs_bench else None
    try:
        for name, fn in tests:
            print(f"== {name} ==")
            fn(src)
    finally:
        if needs_bench:
            src.off()
            with tsctl.LectroTIC4() as tic:
                tstest.configure(tic)

    print(f"\n{'PASS' if not FAILURES else 'FAIL'}: {len(FAILURES)} failed check(s)")
    for f in FAILURES:
        print(f"  - {f}")
    return 1 if FAILURES else 0


if __name__ == "__main__":
    raise SystemExit(main())
