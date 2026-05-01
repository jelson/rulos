#!/usr/bin/env python3

"""Configure a LectroTIC-4 timestamper over its USB CDC SCPI interface.

CLI usage examples:
  tsctl.py idn
  tsctl.py slope 1 NEG
  tsctl.py slope 2          # query
  tsctl.py div 3 100
  tsctl.py output ON
  tsctl.py reset
  tsctl.py raw '*IDN?'
  tsctl.py stream
  tsctl.py --port /dev/ttyACM1 --leave-off slope 1 BOTH

As a library, open a Timestamper instance and call methods on it:

  from tsctl import Timestamper
  with Timestamper() as ts:                  # autodetect
      print(ts.idn())
      ts.set_slope(1, "BOTH")
      ts.set_format(binary=True)
      ts.set_stream_enabled(True)
      for rec in ts.read_for(5.0, binary=True):
          ...
"""

import argparse
import serial
import serial.tools.list_ports
import sys
import time

TIMESTAMPER_VID = 0x1209  # pid.codes
TIMESTAMPER_PID = 0x71C4  # LectroTIC-4
IDN_PREFIX = "Lectrobox,Timestamper"

_BINARY_RECORD_LEN = 8
_NS_PER_TICK = 4
_COUNTER_CHAN_SHIFT = 30
_COUNTER_CHAN_MASK = 0x3FFFFFFF


def autodetect_port():
    """Return the device path of the (single) attached LectroTIC-4."""
    candidates = [p.device for p in serial.tools.list_ports.comports()
                  if p.vid == TIMESTAMPER_VID and p.pid == TIMESTAMPER_PID]
    if not candidates:
        sys.exit(f"No timestamper found (VID:PID "
                 f"{TIMESTAMPER_VID:04x}:{TIMESTAMPER_PID:04x}). "
                 f"Try --port.")
    matches = []
    for dev in candidates:
        try:
            ser = serial.Serial(dev, timeout=0.5)
        except serial.SerialException as e:
            print(f"  {dev}: {e}", file=sys.stderr)
            continue
        try:
            # Quiet stream so the *IDN? response comes back clean.
            ser.write(b"OUTP:STAT OFF\n")
            ser.flush()
            _drain_raw(ser)
            ser.write(b"*IDN?\n")
            ser.flush()
            idn = ser.readline().decode(errors="replace").strip()
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


def _drain_raw(ser, quiet_s=0.1, max_s=1.0):
    """Read until the line goes quiet for quiet_s, or max_s elapses.
    Standalone helper used by autodetect_port before a Timestamper
    instance exists."""
    deadline = time.monotonic() + max_s
    last_rx = time.monotonic()
    saved = ser.timeout
    ser.timeout = 0.05
    try:
        while time.monotonic() < deadline:
            data = ser.read(4096)
            if data:
                last_rx = time.monotonic()
            elif time.monotonic() - last_rx > quiet_s:
                return
    finally:
        ser.timeout = saved


class Timestamper:
    """A connected LectroTIC-4. Pass port=... to override autodetect.
    Use as a context manager (preferred) or call close() explicitly."""

    def __init__(self, port=None, timeout=0.5):
        self._ser = serial.Serial(port or autodetect_port(), timeout=timeout)

    # ---- Lifecycle ----------------------------------------------------

    @property
    def port(self):
        return self._ser.port

    def close(self):
        self._ser.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # ---- Low-level SCPI I/O -------------------------------------------

    def send(self, cmd):
        """Send a single SCPI command (newline appended)."""
        self._ser.write((cmd + "\n").encode())
        self._ser.flush()

    def drain(self, quiet_s=0.1, max_s=1.0):
        """Read until the line goes quiet for quiet_s, or max_s elapses."""
        _drain_raw(self._ser, quiet_s=quiet_s, max_s=max_s)

    def query(self, cmd):
        """Drain, send, and read one line of response. Returns string."""
        self.drain()
        self.send(cmd)
        self._ser.timeout = 0.5
        return self._ser.readline().decode(errors="replace").strip()

    def reset_input_buffer(self):
        """Discard anything pending on the OS-level RX buffer."""
        self._ser.reset_input_buffer()

    # ---- SCPI conveniences --------------------------------------------

    def idn(self):
        return self.query("*IDN?")

    def reset(self):
        self.send("*RST")

    def clear_errors(self):
        self.send("*CLS")

    def get_error(self):
        return self.query("SYST:ERR?")

    def set_slope(self, channel, slope):
        self.send(f"INP{channel}:SLOP {slope.upper()}")

    def get_slope(self, channel):
        return self.query(f"INP{channel}:SLOP?")

    def set_divider(self, channel, n):
        self.send(f"INP{channel}:DIV {n}")

    def get_divider(self, channel):
        return self.query(f"INP{channel}:DIV?")

    def set_stream_enabled(self, on):
        self.send(f"OUTP:STAT {'ON' if on else 'OFF'}")

    def get_stream_enabled(self):
        return self.query("OUTP:STAT?") == "1"

    def set_format(self, binary):
        """Switch wire format. Drains any in-flight bytes so the caller
        can immediately start reading the new format."""
        self.send(f"FORM:DATA {'BIN' if binary else 'TEXT'}")
        self.drain()

    def get_format(self):
        return self.query("FORM:DATA?")

    # ---- Streaming ----------------------------------------------------

    def _iter_binary(self, deadline):
        leftover = b""
        while True:
            if deadline is not None and time.monotonic() >= deadline:
                return
            data = self._ser.read(4096)
            if not data:
                continue
            buf = leftover + data
            n = len(buf) // _BINARY_RECORD_LEN
            for i in range(n):
                off = i * _BINARY_RECORD_LEN
                sec = int.from_bytes(buf[off:off + 4], "little")
                cnt = int.from_bytes(buf[off + 4:off + 8], "little")
                chan = (cnt >> _COUNTER_CHAN_SHIFT) + 1
                ticks = cnt & _COUNTER_CHAN_MASK
                yield ("ts", chan, sec, ticks * _NS_PER_TICK)
            leftover = buf[n * _BINARY_RECORD_LEN:]

    def _iter_text(self, deadline):
        while True:
            if deadline is not None and time.monotonic() >= deadline:
                return
            line = self._ser.readline().decode(errors="replace").strip()
            if not line:
                continue
            if line.startswith("#"):
                yield ("comment", line)
                continue
            parts = line.split()
            if len(parts) != 2:
                continue
            try:
                chan = int(parts[0])
                sec_str, ns_str = parts[1].split(".")
                yield ("ts", chan, int(sec_str), int(ns_str))
            except (ValueError, IndexError):
                continue

    def iter_records(self, binary):
        """Generator yielding records from a streaming device. Yields:
            ('ts', channel:int, seconds:int, nanoseconds:int)
            ('comment', text:str)              # text mode only
        Loops forever; caller breaks out when done. Streaming
        (OUTP:STAT ON) and the format must already be set."""
        return self._iter_binary(None) if binary else self._iter_text(None)

    def read_for(self, duration_s, binary):
        """Like iter_records, but returns after duration_s seconds of
        wall clock even if the device is silent. Sets the underlying
        ser.timeout small enough that the wakeup granularity tracks
        the deadline."""
        self._ser.timeout = 0.05
        deadline = time.monotonic() + duration_s
        return (self._iter_binary(deadline) if binary
                else self._iter_text(deadline))


# ---- CLI ------------------------------------------------------------------

def cmd_idn(ts, args):
    print(ts.idn())


def cmd_reset(ts, args):
    ts.reset()


def cmd_slope(ts, args):
    if args.value is None:
        print(ts.get_slope(args.channel))
    else:
        ts.set_slope(args.channel, args.value)


def cmd_div(ts, args):
    if args.n is None:
        print(ts.get_divider(args.channel))
    else:
        ts.set_divider(args.channel, args.n)


def cmd_output(ts, args):
    if args.state is None:
        print("ON" if ts.get_stream_enabled() else "OFF")
    else:
        # Override the auto-restore behavior: the explicit value wins.
        on = args.state.upper() == "ON"
        args.final_output_state = "ON" if on else "OFF"
        ts.set_stream_enabled(on)


def cmd_raw(ts, args):
    cmd = args.command
    if "?" in cmd:
        print(ts.query(cmd))
    else:
        ts.send(cmd)


def cmd_stream(ts, args):
    binary = (args.format == "binary")
    ts.set_format(binary)
    ts.set_stream_enabled(True)
    args.final_output_state = "ON"
    try:
        for rec in ts.iter_records(binary):
            if rec[0] == "ts":
                _, chan, sec, ns = rec
                sys.stdout.write(f"{chan} {sec}.{ns:09d}\n")
            else:
                sys.stdout.write(rec[1] + "\n")
            sys.stdout.flush()
    except KeyboardInterrupt:
        pass
    finally:
        # Always restore TEXT so other consumers (cat, screen, etc.)
        # see the documented default after we exit.
        ts.send("FORM:DATA TEXT")


def main():
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default=None,
                   help="Serial port (default: autodetect via USB VID/PID + "
                        "*IDN? probe)")
    p.add_argument("--leave-off", action="store_true",
                   help="Leave the timestamp stream disabled after the "
                        "command (default: re-enable on exit)")
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

    sp = sub.add_parser("raw",
                        help="Send a raw SCPI command (queries auto-read)")
    sp.add_argument("command")
    sp.set_defaults(func=cmd_raw)

    sp = sub.add_parser("stream",
                        help="Forward timestamps to stdout until ^C")
    sp.add_argument("format", nargs="?", default="binary",
                    choices=["binary", "text"],
                    help="Wire format to request from the device "
                         "(default: binary; decoded back to ASCII for "
                         "stdout)")
    sp.set_defaults(func=cmd_stream)

    args = p.parse_args()
    args.final_output_state = "OFF" if args.leave_off else "ON"

    with Timestamper(args.port) as ts:
        ts.set_stream_enabled(False)
        ts.drain()
        args.func(ts, args)
        ts.send(f"OUTP:STAT {args.final_output_state}")


if __name__ == "__main__":
    main()
