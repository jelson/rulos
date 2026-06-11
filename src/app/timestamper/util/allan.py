#!/usr/bin/env python3

"""Oscillator-stability analyzer (Allan deviation) built on the LectroTIC-4.

Feed any periodic signal -- an oscillator's divided output, a GPS PPS, a function generator --
into one channel and this computes the overlapping Allan deviation of that signal against the
instrument's 10 MHz reference. The capture IS the phase record: every timestamp is a direct
time-error sample with 4 ns resolution, so no mixer, counter dead-time correction, or
heterodyne stage is involved. With a rubidium or GPS-disciplined reference, the result is the
device-under-test's own stability for taus above ~10 ms.

The channel divider is auto-ranged (binary search, see tsctl) so the phase-sample stream
fits the gate budget regardless of input frequency; the divider sets the smallest tau and the
capture duration bounds the largest. A run
must be loss-free to be meaningful, so any device-reported loss aborts the analysis.

Usage:
  allan.py                          analyze channel 0 for 30 seconds
  allan.py --channel 1 --duration 120
"""

import argparse
import math

import tsctl


def adev_table(x_ns, tau0_ns):
    """Overlapping Allan deviation of the time-error series x_ns (ns, sampled every tau0_ns).
    Returns [(tau_s, adev, n_terms)] over a 1-2-5 tau ladder, requiring at least 8 second-
    difference terms per point."""
    n = len(x_ns)
    ladder = [k * 10**e for e in range(10) for k in (1, 2, 5)]
    table = []
    for m in ladder:
        terms = n - 2 * m
        if terms < 8:
            break
        s = 0.0
        for i in range(terms):
            d = x_ns[i + 2 * m] - 2 * x_ns[i + m] + x_ns[i]
            s += d * d
        tau_ns = m * tau0_ns
        table.append((tau_ns / tsctl.NS, math.sqrt(s / (2 * terms)) / tau_ns, terms))
    return table


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
        help="input channel to analyze (default 0)",
    )
    p.add_argument(
        "--duration",
        type=float,
        default=30.0,
        help="capture duration in seconds (default 30; sets the largest tau)",
    )
    args = p.parse_args()

    with tsctl.LectroTIC4(args.port) as tic:
        print(f"# {tic.idn()}")
        tic.set_slope(args.channel, "POS")
        try:
            freq, divider = tic.autorange_and_acquire([args.channel])[args.channel]
            if freq is None:
                print(f"# no signal on channel {args.channel}")
                return 1
            tau0_s = divider / freq
            print(
                f"# channel {args.channel}: {tsctl.format_freq(freq, int(0.3 * tsctl.NS))}, "
                f"divider {divider}, tau0 {tsctl.format_time_ns(tau0_s * tsctl.NS)}"
            )

            # One continuous capture: tau ladders are built from second differences of a single
            # unbroken phase record, so this can't be stitched from separate marker-synced gates.
            # Twice the worst-case record rate autorange_and_acquire can settle on.
            budget = int(2 * tsctl.TOTAL_BUDGET_HZ * args.duration)
            times = []
            lost = False
            for r in tic.read_for(args.duration):
                if isinstance(r, tsctl.Timestamp):
                    if r.channel == args.channel:
                        times.append(r.seconds * tsctl.NS + r.nanoseconds)
                        if len(times) % 1024 == 0:
                            tsctl.status(
                                f"capturing... {len(times)} samples, "
                                f"{(times[-1] - times[0]) / tsctl.NS:.1f} s"
                            )
                        if len(times) > budget:
                            print("\n# input rate left the divider's range mid-capture. aborting.")
                            return 1
                elif isinstance(r, tsctl.PulsesLost):
                    lost = True
            print()
            if lost:
                print("# device reported loss mid-capture; analysis would be invalid. aborting.")
                return 1
            if len(times) < 32:
                print(f"# only {len(times)} samples captured; not enough to analyze")
                return 1

            # Time-error series: each sample's deviation from a perfect grid at the mean period.
            # Subtracting the mean period only removes the frequency offset (which the Allan
            # second difference cancels anyway); it keeps x small so float math stays exact.
            t0 = times[0]
            mean_tick = (times[-1] - t0) / (len(times) - 1)
            x = [(t - t0) - i * mean_tick for i, t in enumerate(times)]
            measured = divider * tsctl.NS / mean_tick
            span_ns = times[-1] - t0
            print(
                f"# {len(times)} samples over {span_ns / tsctl.NS:.1f} s, "
                f"mean frequency {tsctl.format_freq(measured, span_ns)}"
            )
            print(f"#\n# {'tau':>10}   {'ADEV':>10}   {'terms':>8}")
            for tau_s, adev, terms in adev_table(x, mean_tick):
                print(f"{tsctl.format_time_ns(tau_s * tsctl.NS):>12}   {adev:10.3e}   {terms:8d}")
            return 0
        finally:
            tic.set_divider(args.channel, 1)


if __name__ == "__main__":
    raise SystemExit(main())
