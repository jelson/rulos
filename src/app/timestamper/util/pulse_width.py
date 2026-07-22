#!/usr/bin/env python3

"""Pulse-width and duty-cycle meter built on the LectroTIC-4.

The instrument latches the polarity of every edge in hardware, so capturing with slope BOTH turns
each pulse into a rising/falling timestamp pair: width is their difference, period is rise-to-
rise, and both come back as exact 4 ns-resolution intervals -- no analog threshold or oscilloscope
cursor involved. The display shows the median width, duty cycle, and repetition rate per gate,
plus the width's spread across the gate.

Pairing rising to falling edges requires every edge, so this mode runs undecimated (the divider
counts edges in BOTH mode and would break pairs). The input repetition rate is therefore capped:
the meter probes the rate first and refuses anything whose edges would blow the gate budget
(MAX_PULSE_HZ). Slow signals stretch the gate automatically, like the frequency counter.

Usage:
  pulse_width.py                     measure channel 0
  pulse_width.py --channel 3
  pulse_width.py --updates 10        exit after 10 readings (for scripting)
"""

import argparse

import tsctl

BASE_GATE_S = 0.35
MAX_GATE_S = 4.0

# Aim for at least this many complete pulses in a gate before reporting.
MIN_PULSES = 8

# Width mode runs undecimated and each pulse costs two records, so the repetition rate is capped
# at half the record-rate budget.
MAX_PULSE_HZ = tsctl.TOTAL_BUDGET_HZ // 2


def pair_pulses(recs):
    """Walk one channel's [(t_ns, polarity)] records in time order, pairing each rising edge with
    the next falling edge. The two polarities arrive as independently-ordered sub-streams, so the
    merged list is sorted first. Returns (widths_ns, periods_ns)."""
    widths, periods = [], []
    prev_rise = None
    expect_fall = False
    for t, pol in sorted(recs):
        if pol == "+":
            if prev_rise is not None:
                periods.append(t - prev_rise)
            prev_rise = t
            expect_fall = True
        elif expect_fall:
            widths.append(t - prev_rise)
            expect_fall = False
    return widths, periods


def median(values):
    return sorted(values)[len(values) // 2]


def main():
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--port", default=None, help="LectroTIC-4 serial port (default: autodetect)")
    p.add_argument(
        "--channel",
        type=int,
        choices=[0, 1, 2, 3],
        default=0,
        help="input channel to measure (default 0)",
    )
    p.add_argument(
        "--updates",
        type=int,
        default=None,
        help="exit after this many readings (default: run until ^C)",
    )
    args = p.parse_args()
    ch = args.channel

    shown = 0
    gate_s = BASE_GATE_S
    ready = False  # rate probed and slope BOTH armed

    with tsctl.LectroTIC4(args.port) as tic:
        print(f"# {tic.idn()}")
        print(f"# measuring channel {ch}; ^C to stop")
        try:
            while args.updates is None or shown < args.updates:
                if not ready:
                    # Probe the repetition rate on rising edges only, auto-ranged from the top, so
                    # the undecimated BOTH capture below is entered knowing it fits the budget.
                    tic.set_slope(ch, "POS")
                    tsctl.status(f"{'ranging...':>16}")
                    freq, _ = tic.autorange_and_acquire([ch])[ch]
                    if freq is not None and freq > MAX_PULSE_HZ:
                        tsctl.reading(
                            f"{'too fast':>16}   [{tsctl.format_freq(freq, 10**6)}: width "
                            f"mode needs every edge; max {MAX_PULSE_HZ} pulses/s]",
                            live=args.updates is None,
                        )
                        shown += 1
                        continue
                    tic.set_divider(ch, 1)
                    tic.set_slope(ch, "BOTH")
                    if freq is not None:
                        gate_s = min(MAX_GATE_S, max(BASE_GATE_S, MIN_PULSES / freq))
                    ready = True

                recs, overrun, aborted = tic.measure_by_channel([ch], gate_s)
                if aborted:
                    ready = False  # rate grew past the budget: re-probe from the top
                    continue
                widths, periods = pair_pulses(recs[ch])

                if widths and periods:
                    w, per = median(widths), median(periods)
                    spread = max(widths) - min(widths)
                    line = (
                        f"width {tsctl.format_time_ns(w):>10}   "
                        f"duty {100 * w / per:6.2f} %   "
                        f"rate {tsctl.format_freq(tsctl.NS / per, per * len(periods)):>12}   "
                        f"[{len(widths)} pulses, spread {tsctl.format_time_ns(spread)}]"
                    )
                    if overrun:
                        line += "   OVERRUN"
                    tsctl.reading(line, live=args.updates is None)
                    shown += 1
                    gate_s = min(MAX_GATE_S, max(BASE_GATE_S, MIN_PULSES * per / tsctl.NS))
                elif gate_s < MAX_GATE_S:
                    gate_s = min(MAX_GATE_S, gate_s * 2)
                    tsctl.status(f"{'measuring...':>16}   [gate {gate_s:.2f} s]")
                else:
                    tsctl.reading(
                        f"{'no signal':>16}   [gate {gate_s:.2f} s]", live=args.updates is None
                    )
                    ready = False  # re-probe in case the signal returns at a new rate
                    shown += 1
        except KeyboardInterrupt:
            pass
        finally:
            print()
            tic.set_slope(ch, "POS")
            tic.set_divider(ch, 1)


if __name__ == "__main__":
    main()
