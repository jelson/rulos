#!/usr/bin/env python3

"""Configure a LectroTIC-4 timestamper over its USB CDC SCPI interface.

Each invocation opens the serial port, disables the timestamp stream
(OUTP:STAT OFF) so it can talk synchronously, runs one or more SCPI
commands, and then either restores streaming or leaves it off.

Examples:
  tsctl.py idn
  tsctl.py slope 1 NEG
  tsctl.py slope 2          # query
  tsctl.py div 3 100
  tsctl.py output ON
  tsctl.py reset
  tsctl.py raw '*IDN?'
  tsctl.py --port /dev/ttyACM1 --leave-off slope 1 BOTH
"""

import argparse
import serial
import serial.tools.list_ports
import sys
import time

TIMESTAMPER_VID = 0x1209  # pid.codes
TIMESTAMPER_PID = 0x71C4  # LectroTIC-4
IDN_PREFIX = "Lectrobox,Timestamper"


def autodetect_port():
    candidates = [p.device for p in serial.tools.list_ports.comports()
                  if p.vid == TIMESTAMPER_VID and p.pid == TIMESTAMPER_PID]
    if not candidates:
        sys.exit(f"No timestamper found (VID:PID "
                 f"{TIMESTAMPER_VID:04x}:{TIMESTAMPER_PID:04x}). Try --port.")
    matches = []
    for dev in candidates:
        try:
            ser = serial.Serial(dev, timeout=0.5)
        except serial.SerialException as e:
            print(f"  {dev}: {e}", file=sys.stderr)
            continue
        try:
            send(ser, "OUTP:STAT OFF")
            drain(ser)
            idn = query(ser, "*IDN?")
            if idn.startswith(IDN_PREFIX):
                matches.append(dev)
        finally:
            ser.close()
    if not matches:
        sys.exit("Found device(s) at the timestamper VID:PID but none "
                 "answered *IDN? as a timestamper.")
    if len(matches) > 1:
        sys.exit(f"Multiple timestampers found: {matches}. Use --port.")
    return matches[0]


def drain(ser, quiet_s=0.1, max_s=1.0):
    """Read until the line goes quiet for quiet_s, or max_s elapses."""
    deadline = time.monotonic() + max_s
    last_rx = time.monotonic()
    ser.timeout = 0.05
    while time.monotonic() < deadline:
        data = ser.read(4096)
        if data:
            last_rx = time.monotonic()
        elif time.monotonic() - last_rx > quiet_s:
            return


def send(ser, cmd):
    ser.write((cmd + "\n").encode())
    ser.flush()


def query(ser, cmd):
    drain(ser)
    send(ser, cmd)
    ser.timeout = 0.5
    line = ser.readline().decode(errors="replace").strip()
    return line


def cmd_idn(ser, args):
    print(query(ser, "*IDN?"))


def cmd_reset(ser, args):
    send(ser, "*RST")


def cmd_slope(ser, args):
    if args.value is None:
        print(query(ser, f"INP{args.channel}:SLOP?"))
    else:
        send(ser, f"INP{args.channel}:SLOP {args.value.upper()}")


def cmd_div(ser, args):
    if args.n is None:
        print(query(ser, f"INP{args.channel}:DIV?"))
    else:
        send(ser, f"INP{args.channel}:DIV {args.n}")


def cmd_output(ser, args):
    if args.state is None:
        on = query(ser, "OUTP:STAT?")
        print("ON" if on == "1" else "OFF")
    else:
        # Override the auto-restore behavior: the explicit value wins.
        args.final_output_state = args.state.upper()
        send(ser, f"OUTP:STAT {args.state.upper()}")


def cmd_raw(ser, args):
    cmd = args.command
    if "?" in cmd:
        print(query(ser, cmd))
    else:
        send(ser, cmd)


def cmd_stream(ser, args):
    # Forward the device's timestamp stream to stdout. Pyserial puts the
    # port in raw / no-echo mode, so this works correctly without an
    # external stty.
    send(ser, "OUTP:STAT ON")
    args.final_output_state = "ON"
    ser.timeout = 0.5
    try:
        while True:
            chunk = ser.read(4096)
            if chunk:
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
    except KeyboardInterrupt:
        pass


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default=None,
                   help="Serial port (default: autodetect via USB VID/PID + "
                        "*IDN? probe)")
    p.add_argument("--leave-off", action="store_true",
                   help="Leave the timestamp stream disabled after the command "
                        "(default: re-enable on exit)")
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("idn", help="Read *IDN? identification string")
    sp.set_defaults(func=cmd_idn)

    sp = sub.add_parser("reset", help="Send *RST (defaults, streaming on)")
    sp.set_defaults(func=cmd_reset)

    sp = sub.add_parser("slope", help="Set or query input slope")
    sp.add_argument("channel", type=int, choices=[1, 2, 3, 4])
    sp.add_argument("value", nargs="?", choices=["pos", "neg", "both",
                                                 "POS", "NEG", "BOTH"],
                    help="Omit to query")
    sp.set_defaults(func=cmd_slope)

    sp = sub.add_parser("div", help="Set or query channel divider (>= 1)")
    sp.add_argument("channel", type=int, choices=[1, 2, 3, 4])
    sp.add_argument("n", nargs="?", type=int, help="Omit to query")
    sp.set_defaults(func=cmd_div)

    sp = sub.add_parser("output", help="Set or query timestamp stream state")
    sp.add_argument("state", nargs="?", choices=["on", "off", "ON", "OFF"],
                    help="Omit to query")
    sp.set_defaults(func=cmd_output)

    sp = sub.add_parser("raw", help="Send a raw SCPI command (queries auto-read)")
    sp.add_argument("command")
    sp.set_defaults(func=cmd_raw)

    sp = sub.add_parser("stream",
                        help="Forward timestamps to stdout until ^C")
    sp.set_defaults(func=cmd_stream)

    args = p.parse_args()
    args.final_output_state = "OFF" if args.leave_off else "ON"

    port = args.port or autodetect_port()
    ser = serial.Serial(port, timeout=0.5)
    try:
        send(ser, "OUTP:STAT OFF")
        drain(ser)
        args.func(ser, args)
        send(ser, f"OUTP:STAT {args.final_output_state}")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
