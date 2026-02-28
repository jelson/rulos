#!/usr/bin/env python3

"""Binary-search for the timestamper's dead time.

Drives a Rigol DG1022Z (via siggen.py's Siggen class) to produce pulse
bursts (3 pulses) at various spacings while reading the timestamper's
serial output to determine whether all pulses in each burst are captured.

Using triples rather than pairs measures the full ISR round-trip time:
the 3rd pulse arrives while the ISR may still be processing the 2nd,
so the constraint is N > (total_ISR_time + time_to_CCR_read) / 2.

Usage:
  deadtime.py                           defaults: 100-1000 ns, /dev/ttyACM0
  deadtime.py --bottom 50 --top 500     custom range in ns
  deadtime.py --port /dev/ttyACM1       custom serial port
  deadtime.py --host siggen2            custom signal generator hostname
"""

import argparse
import serial
import sys
import time
from dataclasses import dataclass

from siggen import Siggen


NCYC = 3  # pulses per burst


@dataclass
class PulseCount:
    timestamps: int    # number of timestamp lines received
    overcaptures: int  # number of "overcapture" comment lines
    complete: int      # bursts where all NCYC pulses were captured


def count_pulses(port, settle_time_s=2.0, measure_time_s=3.0, verbose=True):
    """Read timestamper output and count complete bursts."""
    ser = serial.Serial(port, timeout=0.5)
    try:
        # Flush any stale data, then let the new pair spacing settle
        ser.reset_input_buffer()
        deadline = time.monotonic() + settle_time_s
        while time.monotonic() < deadline:
            ser.readline()

        # Now measure
        ser.reset_input_buffer()
        timestamps = []
        overcaptures = 0
        deadline = time.monotonic() + measure_time_s
        while time.monotonic() < deadline:
            line = ser.readline().decode(errors="replace").strip()
            if not line:
                continue
            if verbose:
                print(f"    | {line}")
            if line.startswith("#"):
                if "overcapture" in line.lower():
                    overcaptures += 1
                continue
            # Timestamp line: "1 123.456789012345" or "2 ..."
            timestamps.append(line)

        # Group timestamps into bursts. Bursts repeat 1/sec, so
        # timestamps within 0.5s of each other belong to the same burst.
        complete = 0
        i = 0
        while i < len(timestamps):
            # Start a new burst with this timestamp
            burst_size = 1
            try:
                t_start = float(timestamps[i].split()[1])
            except (IndexError, ValueError):
                i += 1
                continue
            j = i + 1
            while j < len(timestamps):
                try:
                    t = float(timestamps[j].split()[1])
                except (IndexError, ValueError):
                    break
                if abs(t - t_start) < 0.5:
                    burst_size += 1
                    j += 1
                else:
                    break
            if burst_size >= NCYC:
                complete += 1
            i = j

        return PulseCount(
            timestamps=len(timestamps),
            overcaptures=overcaptures,
            complete=complete,
        )

    finally:
        ser.close()




def test_spacing(sg, port, ns, verbose=True):
    """Set the signal generator to the given spacing and check results.

    Returns True if all pulses in each burst are reliably captured
    (no dead time issue), False if pulses are being missed.
    """
    print(f"  Testing {ns:.1f} ns...")

    sg.pulse_burst(ns)

    # Read timestamper output
    result = count_pulses(port, verbose=verbose)

    # We expect ~3 bursts in the 3-second measurement window.
    # "pass" = we see pairs (both pulses captured) and no overcaptures.
    # "fail" = overcaptures reported OR we never see pairs (second pulse
    # is lost entirely, which at very short spacings means the hardware
    # overwrites CCR before the ISR can read it).
    passed = result.complete >= 2 and result.overcaptures == 0

    status = "PASS" if passed else "FAIL"
    print(f"  {ns:6.1f} ns: {status}  "
          f"(timestamps={result.timestamps}, complete={result.complete}, "
          f"overcaptures={result.overcaptures})")

    return passed


def main():
    parser = argparse.ArgumentParser(
        description="Binary-search for timestamper dead time")
    parser.add_argument("--bottom", type=float, default=100,
                        help="Lower bound in ns (default: 100)")
    parser.add_argument("--top", type=float, default=1000,
                        help="Upper bound in ns (default: 1000)")
    parser.add_argument("--port", default="/dev/ttyACM0",
                        help="Timestamper serial port (default: /dev/ttyACM0)")
    parser.add_argument("--host", default="siggen",
                        help="Signal generator hostname (default: siggen)")
    parser.add_argument("--precision", type=float, default=5,
                        help="Stop when range narrows to this many ns (default: 5)")
    parser.add_argument("--quiet", "-q", action="store_true",
                        help="Suppress raw serial output")
    args = parser.parse_args()

    lo = args.bottom
    hi = args.top
    verbose = not args.quiet

    print(f"Searching for dead time in range [{lo:.0f}, {hi:.0f}] ns")
    print(f"Timestamper port: {args.port}")
    print(f"Signal generator: {args.host}")
    print()

    with Siggen(host=args.host) as sg:
        print(f"Connected to: {sg.idn()}")
        print()

        # Verify endpoints: top should pass, bottom should fail
        print("Verifying upper bound...")
        if not test_spacing(sg, args.port, hi, verbose):
            print(f"ERROR: Upper bound {hi:.0f} ns fails -- increase --top")
            sys.exit(1)

        print("Verifying lower bound...")
        if test_spacing(sg, args.port, lo, verbose):
            print(f"NOTE: Lower bound {lo:.0f} ns already passes -- "
                  f"decrease --bottom or dead time is below {lo:.0f} ns")
            sg.output_off()
            sys.exit(0)

        print()
        print("Binary search:")

        while (hi - lo) > args.precision:
            mid = (lo + hi) / 2
            # Round to nearest ns for the signal generator
            mid = round(mid)
            if mid == lo or mid == hi:
                break

            if test_spacing(sg, args.port, mid, verbose):
                hi = mid
            else:
                lo = mid

        print()
        print(f"Dead time is between {lo:.0f} ns and {hi:.0f} ns")

        sg.output_off()
        print("Signal generator output off.")


if __name__ == "__main__":
    main()
