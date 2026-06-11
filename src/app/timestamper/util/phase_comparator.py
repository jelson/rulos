#!/usr/bin/env python3

"""Two-channel frequency-ratio and phase comparator built on the LectroTIC-4.

Both channels are timestamped by the same 250 MHz counter, phase-locked to the lab's 10 MHz
reference, so comparing two signals needs no mixers, no dividers, and no common trigger: each
gate yields both channels' frequencies from pure arithmetic, their ratio to ~1e-9, and -- when
the frequencies match -- the phase offset between the trains and its drift rate.

The phase readout pairs each channel-B record with the nearest channel-A record and reduces the
difference modulo the signal period, so it is independent of where each channel's divider
happens to sit in the pulse train. Drift is a least-squares slope across the recent gates'
offsets; for two locked sources it reads ~0 ns/s, and for independent sources it reads their
fractional frequency difference directly (1 ns/s = 1 ppb).

Usage:
  phase_comparator.py                     compare channel 0 (A) against channel 1 (B)
  phase_comparator.py --channels 2 3
  phase_comparator.py --updates 10        exit after 10 readings (for scripting)
"""

import argparse
import bisect

import tsctl

GATE_S = 0.5

# Treat the two inputs as nominally equal (enabling the phase readout) when their measured
# frequencies agree this closely.
EQUAL_BAND = 1e-3

# Gates of (time, unwrapped offset) history the drift fit looks back over.
DRIFT_HISTORY = 40


def phase_offset_ns(times_a, times_b, period_ns):
    """Median phase offset of train B relative to train A, in (-period/2, +period/2]: each B
    record minus its nearest A record, reduced modulo the period. Divider decimation only changes
    WHICH edges each channel reports, so the reduction recovers the same offset regardless of how
    the two dividers align."""
    offsets = []
    for tb in times_b:
        i = bisect.bisect_left(times_a, tb)
        candidates = [times_a[j] for j in (i - 1, i) if 0 <= j < len(times_a)]
        d = min((tb - ta for ta in candidates), key=abs)
        offsets.append((d + period_ns / 2) % period_ns - period_ns / 2)
    offsets.sort()
    return offsets[len(offsets) // 2]


def drift_ns_per_s(history):
    """Least-squares slope of (time_s, offset_ns) pairs: phase drift in ns/s (numerically equal
    to the fractional frequency difference in ppb)."""
    n = len(history)
    mean_t = sum(t for t, _ in history) / n
    mean_x = sum(x for _, x in history) / n
    num = sum((t - mean_t) * (x - mean_x) for t, x in history)
    den = sum((t - mean_t) ** 2 for t, _ in history)
    return num / den if den else 0.0


def main():
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--port", default=None, help="LectroTIC-4 serial port (default: autodetect)")
    p.add_argument(
        "--channels",
        type=int,
        nargs=2,
        choices=[0, 1, 2, 3],
        default=[0, 1],
        metavar=("A", "B"),
        help="the two input channels to compare (default: 0 1)",
    )
    p.add_argument(
        "--updates",
        type=int,
        default=None,
        help="exit after this many readings (default: run until ^C)",
    )
    args = p.parse_args()
    ch_a, ch_b = args.channels
    if ch_a == ch_b:
        p.error("the two channels must differ")

    shown = 0
    history = []  # (gate midpoint seconds, unwrapped offset ns)

    with tsctl.LectroTIC4(args.port) as tic:
        print(f"# {tic.idn()}")
        print(f"# comparing channel {ch_b} (B) against channel {ch_a} (A); ^C to stop")
        for ch in (ch_a, ch_b):
            tic.set_slope(ch, "POS")
        try:
            divider = {ch_a: 1, ch_b: 1}
            while args.updates is None or shown < args.updates:
                for ch in (ch_a, ch_b):
                    tic.set_divider(ch, divider[ch])
                recs, overrun, aborted = tic.measure_by_channel([ch_a, ch_b], GATE_S)
                times = {ch: [t for t, _ in recs[ch]] for ch in (ch_a, ch_b)}

                # Binary-search ranging, per channel: over-budget gates abort in milliseconds and
                # double the offending divider; an empty channel halves back toward divider 1.
                changed = False
                for ch in (ch_a, ch_b):
                    if len(times[ch]) > tsctl.TOTAL_BUDGET_HZ * GATE_S / 2:
                        divider[ch] = min(tsctl.MAX_DIVIDER, divider[ch] * 2)
                        changed = True
                    elif len(times[ch]) < 2 and divider[ch] > 1:
                        divider[ch] //= 2
                        changed = True
                if changed:
                    tsctl.status(
                        f"{'ranging...':>16}   [dividers {divider[ch_a]}, {divider[ch_b]}]"
                    )
                    continue

                freq_a, span_a = tsctl.span_freq(times[ch_a], divider[ch_a])
                freq_b, span_b = tsctl.span_freq(times[ch_b], divider[ch_b])
                if freq_a is None or freq_b is None:
                    missing = [f"ch{ch}" for ch, f in ((ch_a, freq_a), (ch_b, freq_b)) if f is None]
                    tsctl.status(f"{'no signal':>16}   [{', '.join(missing)}]")
                    history.clear()
                    if args.updates is not None:
                        print()
                        shown += 1
                    continue

                ratio = freq_b / freq_a
                line = (
                    f"A {tsctl.format_freq(freq_a, span_a):>14}   "
                    f"B {tsctl.format_freq(freq_b, span_b):>14}   "
                    f"B/A {ratio:.9f}"
                )

                if abs(ratio - 1) < EQUAL_BAND:
                    period_ns = 2 * tsctl.NS / (freq_a + freq_b)
                    offset = phase_offset_ns(times[ch_a], times[ch_b], period_ns)
                    # Unwrap against the previous gate so a slewing offset accumulates instead of
                    # jumping by a period, then fit drift across the recent history.
                    if history:
                        offset += round((history[-1][1] - offset) / period_ns) * period_ns
                    mid_s = (times[ch_a][0] + times[ch_a][-1]) / 2 / tsctl.NS
                    history.append((mid_s, offset))
                    del history[:-DRIFT_HISTORY]
                    line += f"   dphi {offset:+8.1f} ns"
                    if len(history) >= 3:
                        line += f"   drift {drift_ns_per_s(history):+7.2f} ns/s"
                else:
                    history.clear()

                if overrun:
                    line += "   OVERRUN"
                tsctl.status(line)
                shown += 1
                if args.updates is not None:
                    print()
        except KeyboardInterrupt:
            pass
        finally:
            print()
            for ch in (ch_a, ch_b):
                tic.set_divider(ch, 1)


if __name__ == "__main__":
    main()
