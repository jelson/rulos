#!/usr/bin/env python3

"""Four-channel auto-ranging frequency counter built on the LectroTIC-4.

The instrument timestamps every edge with 4 ns resolution against the lab's 10 MHz reference, so
frequency falls out of pure arithmetic: divider * (n - 1) / (t_last - t_first) over a short gate,
computed on integer nanoseconds. Accuracy is the reference's accuracy; resolution improves linearly
with gate time (a 0.35 s gate resolves ~10 parts per billion). All four channels are counted at
once and displayed one per line -- redrawn in place on a live terminal, printed as plain blocks
when --updates is given -- with each channel always showing its own state: a reading, ranging,
measuring (while the gate stretches), or no signal.

Auto-ranging is a binary search on each channel's divider, sharing the total record-rate budget
(see tsctl) evenly across the four channels. A gate is abandoned the moment it crosses the
budget, so over-budget probes cost milliseconds: an over-share channel's divider doubles until
its gate fits, and any gate that fits is a valid reading -- at that record rate the wire keeps up
edge-for-edge, and resolution depends only on the gate span, not the record count. An empty
channel halves its divider to re-acquire; when every channel is silent the gate stretches for
slow signals instead, trading update rate for resolution exactly like a bench counter's gate
control. Inputs beyond the device's maximum capture rate are out of range. Device-reported loss
is flagged OVERRUN.

Usage:
  freq_counter.py                    count all four channels, update a few times per second
  freq_counter.py --updates 10       exit after 10 readings (for scripting)
"""

import argparse

import tsctl

CHANNELS = (0, 1, 2, 3)

BASE_GATE_S = 0.35  # a few updates per second
MAX_GATE_S = 4.0  # slow-signal limit: down to ~0.5 Hz before "no signal"


def show_block(lines, live):
    """Display one update: in live mode the block redraws in place (cursor returns to its top so
    the next update overwrites it); otherwise it prints as a plain newline-separated block."""
    if live:
        print(
            "".join(f"\r{line}\x1b[K\n" for line in lines) + f"\x1b[{len(lines)}A",
            end="",
            flush=True,
        )
    else:
        print("\n".join(lines) + "\n", flush=True)


def main():
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--port", default=None, help="LectroTIC-4 serial port (default: autodetect)")
    p.add_argument(
        "--updates",
        type=int,
        default=None,
        help="exit after this many readings (default: run until ^C)",
    )
    args = p.parse_args()

    divider = {ch: 1 for ch in CHANNELS}
    gate_s = BASE_GATE_S
    shown = 0
    live = args.updates is None

    with tsctl.LectroTIC4(args.port) as tic:
        print(f"# {tic.idn()}")
        print("# counting all channels; ^C to stop")
        for ch in CHANNELS:
            tic.set_slope(ch, "POS")
        try:
            while args.updates is None or shown < args.updates:
                for ch in CHANNELS:
                    tic.set_divider(ch, divider[ch])
                recs, overrun, _ = tic.measure_by_channel(CHANNELS, gate_s)
                times = {ch: [t for t, _ in recs[ch]] for ch in CHANNELS}

                # Binary-search ranging against each channel's share of the budget: over-share
                # channels abort the gate in milliseconds and double; empty channels halve back.
                share = tsctl.TOTAL_BUDGET_HZ * gate_s / len(CHANNELS)
                changed = set()
                for ch in CHANNELS:
                    if len(times[ch]) > share:
                        divider[ch] = min(tsctl.MAX_DIVIDER, divider[ch] * 2)
                        changed.add(ch)
                    elif len(times[ch]) < 2 and divider[ch] > 1:
                        divider[ch] //= 2
                        changed.add(ch)

                # When every channel is silent (dividers all at 1), stretch the gate for slow
                # signals before declaring silence.
                all_silent = all(len(times[ch]) < 2 for ch in CHANNELS)
                stretching = not changed and all_silent and gate_s < MAX_GATE_S
                if stretching:
                    gate_s = min(MAX_GATE_S, gate_s * 2)

                lines = []
                for ch in CHANNELS:
                    freq, span_ns = tsctl.span_freq(times[ch], divider[ch])
                    if ch in changed:
                        lines.append(f"ch{ch} {'ranging...':>14}   [divider -> {divider[ch]}]")
                    elif freq is not None:
                        lines.append(
                            f"ch{ch} {tsctl.format_freq(freq, span_ns):>14}   "
                            f"[divider {divider[ch]}]"
                        )
                    elif stretching:
                        lines.append(f"ch{ch} {'measuring...':>14}")
                    else:
                        lines.append(f"ch{ch} {'no signal':>14}")
                lines.append(f"[gate {gate_s:.2f} s]" + ("  OVERRUN" if overrun else ""))

                # Ranging and gate-stretch passes are transient: they redraw the live display but
                # don't count as (or print) an update.
                transient = bool(changed) or stretching
                if live:
                    show_block(lines, live=True)
                if not transient:
                    shown += 1
                    if not live:
                        show_block(lines, live=False)
                # Snap the gate back only when records are plentiful; slow signals keep their
                # stretched gate instead of re-discovering it after every reading.
                if max(len(times[ch]) for ch in CHANNELS) >= 8:
                    gate_s = BASE_GATE_S
        except KeyboardInterrupt:
            pass
        finally:
            if live:
                print("\n" * 5, end="")
            for ch in CHANNELS:
                tic.set_divider(ch, 1)


if __name__ == "__main__":
    main()
