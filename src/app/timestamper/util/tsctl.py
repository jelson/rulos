#!/usr/bin/env python3

"""Configure a LectroTIC-4 timestamper over its USB CDC SCPI interface.

CLI usage examples:
  tsctl.py idn
  tsctl.py slope 0 NEG
  tsctl.py slope 1          # query
  tsctl.py div 2 100
  tsctl.py format binary
  tsctl.py reset
  tsctl.py raw '*IDN?'
  tsctl.py stream
  tsctl.py --port /dev/ttyACM1 slope 0 BOTH

As a library, open a Timestamper instance and call methods on it.
Streaming is implicitly handled: query()/get_*() briefly silence the
stream around the wire exchange so SCPI responses come back clean,
and read_for()/iter_records() ensure the stream is on before reading.

  from tsctl import Timestamper

  # Default: autodetect by USB VID/PID, BINARY wire format. Override
  # with port="/dev/ttyACM1" or binary=False.
  with Timestamper() as ts:
      print(ts.idn())                         # "Lectrobox,Timestamper,..."
      ts.reset()                              # *RST

      # Per-channel input configuration (channel ∈ 0..3).
      ts.set_slope(0, "POS")                  # rising edges only
      ts.set_slope(1, "NEG")                  # falling edges only
      ts.set_slope(2, "BOTH")                 # every transition
      ts.set_divider(0, 100)                  # report every 100th edge

      # Read configuration back.
      print(ts.get_slope(0))                  # "POS"
      print(ts.get_divider(0))                # "100"

      # Wire format. The default is binary; switch to ASCII if you
      # want to read the stream as text.
      ts.set_binary(False)

      # Discard anything the device has already sent (overflow comments,
      # records buffered under the old configuration, etc.) so the next
      # read starts on fresh output.
      ts.reset_input_buffer()

      # Capture for 5 seconds. Loops forever via iter_records() if you
      # want an unbounded stream instead.
      for r in ts.read_for(5.0):
          if r.kind == "ts":
              print(f"ch {r.channel}: t = {r.seconds}.{r.nanoseconds:09d} s")
          # else r.kind == "comment" (TEXT mode only): r.comment

      # Latched-error access for diagnostics.
      ts.clear_errors()                       # *CLS
      print(ts.get_error())                   # "0,\"No error\""

      # Escape hatches for raw SCPI.
      ts.send("FOO:BAR 1")                    # no response read
      print(ts.query("FOO:BAR?"))             # returns one line
"""

import argparse
from collections import namedtuple
import serial
import serial.tools.list_ports
import sys
import time

# Yielded by Timestamper.iter_records / read_for. Exactly one of
#   * (kind="ts", channel, seconds, nanoseconds, comment=None)
#   * (kind="comment", channel=None, seconds=None, nanoseconds=None, comment)
# is populated. `seconds` and `nanoseconds` are integers (whole seconds
# since the device booted, plus the 0..999_999_999 ns within that
# second), kept separate on purpose: a single 64-bit float can't hold
# enough distinct nanoseconds to survive runs past ~104 days, so the
# library never collapses a timestamp into one. Use record_seconds()
# for relative math over short windows where a float is fine.
Record = namedtuple("Record", "kind channel seconds nanoseconds comment")


def record_seconds(r):
    """Float seconds-since-boot for a 'ts' Record. Convenient for
    relative measurements over short windows (gaps, burst durations).
    Do NOT use it to store or compare absolute timestamps from runs
    longer than ~104 days: that is exactly the 2**53-nanosecond
    float64 limit r.seconds/r.nanoseconds exist to avoid."""
    return r.seconds + r.nanoseconds * 1e-9

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
    Use as a context manager (preferred) or call close() explicitly.

    Stream wire format defaults to BINARY; set ts.set_binary(False) to
    switch to ASCII. The device and the host's record parser are kept
    in sync automatically."""

    def __init__(self, port=None, timeout=0.5, binary=True):
        self._ser = serial.Serial(port or autodetect_port(), timeout=timeout)
        self._binary = bool(binary)
        # The library's contract is that streaming is on at entry and
        # at exit. Internally, query() briefly disables the stream so
        # the response comes back clean. We track that state in
        # _stream_on so query() can avoid the toggle in the rare case
        # a caller explicitly disables streaming for testing.
        self._stream_on = True
        self.send("OUTP:STAT ON")
        self.send(f"FORM:DATA {'BIN' if self._binary else 'TEXT'}")
        self.reset_input_buffer()

    # ---- Lifecycle ----------------------------------------------------

    @property
    def port(self):
        return self._ser.port

    def close(self):
        """Restore streaming to ON (the documented default the library
        promises) and release the serial port. Other device state
        (slope, divider, format) is left as-is."""
        try:
            if not self._stream_on:
                self.set_stream_enabled(True)
        except Exception:
            pass
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

    def reset_input_buffer(self, settle_s=0.2):
        """Discard whatever the device has already sent so the next
        read starts on fresh output. Sleeps settle_s to let the device
        finish draining anything buffered under the previous state,
        then drops the OS-level RX buffer. After this returns, every
        byte that arrives is post-call."""
        time.sleep(settle_s)
        self._ser.reset_input_buffer()

    def query(self, cmd):
        """Send a query and read one line of response. If streaming
        is on (the normal state), briefly disable it so the response
        is not interleaved with timestamp output, then re-enable. If
        the caller has explicitly silenced the stream for a series of
        queries, leave it silent."""
        toggle = self._stream_on
        if toggle:
            self.send("OUTP:STAT OFF")
        # Just-sent OUTP:STAT OFF takes effect within USB latency; a
        # short settle is enough to drop the few in-flight records.
        self.reset_input_buffer(settle_s=0.01)
        self.send(cmd)
        self._ser.timeout = 0.5
        line = self._ser.readline().decode(errors="replace").strip()
        if toggle:
            self.send("OUTP:STAT ON")
        return line

    def read_raw(self, duration_s):
        """Read whatever bytes arrive for duration_s seconds and return
        them. Does not parse, decode, or enable streaming -- useful for
        verifying that the device is silent (e.g. while OUTP:STAT is OFF)."""
        self._ser.timeout = 0.05
        deadline = time.monotonic() + duration_s
        out = bytearray()
        while time.monotonic() < deadline:
            chunk = self._ser.read(4096)
            if chunk:
                out += chunk
        return bytes(out)

    # ---- SCPI conveniences --------------------------------------------

    def idn(self):
        return self.query("*IDN?")

    def reset(self):
        """*RST. The device returns to TEXT mode and streaming on; we
        update the host cache and re-assert our chosen wire format so
        host and device stay in sync."""
        self.send("*RST")
        self._stream_on = True
        if self._binary:
            self.send("FORM:DATA BIN")
            self.reset_input_buffer()

    def discard_pending(self):
        """Drop everything currently in flight so the next read sees
        only post-discard records. Channel configuration is preserved.

        Synchronizes via a firmware-emitted marker, no sleeps:

            1. silence the stream (so no records emit after the marker)
            2. send OUTPut:CLEar; the firmware clears its ring and
               emits "# output discarded\\n" straight to USB CDC
            3. read bytes until that marker appears, dropping everything
            4. restore the prior stream state
        """
        was_on = self._stream_on
        self.set_stream_enabled(False)
        self.send("OUTP:CLE")

        marker = b"# output discarded\n"
        buf = bytearray()
        # Short timeout so the read returns promptly with whatever's
        # in the OS RX buffer; we don't want to wait for a full 4096-byte
        # chunk when the marker is the last thing the device emits.
        self._ser.timeout = 0.01
        deadline = time.monotonic() + 1.0
        while marker not in buf:
            if time.monotonic() > deadline:
                raise RuntimeError(
                    "OUTP:CLE did not produce its marker within 1 s — "
                    "is the device responsive?")
            chunk = self._ser.read(4096)
            if chunk:
                buf += chunk

        if was_on:
            self.set_stream_enabled(True)

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
        """Enable or disable continuous timestamp output. Normally the
        library manages this for you (queries briefly disable, exit
        re-enables); call this only to explicitly silence the device
        in the middle of a session, e.g. for behavioral testing."""
        self.send(f"OUTP:STAT {'ON' if on else 'OFF'}")
        self._stream_on = bool(on)

    def get_stream_enabled(self):
        """Returns the host-side cached stream state. Avoids querying
        OUTP:STAT? on the wire (which query() would itself toggle and
        thus return a misleading answer)."""
        return self._stream_on

    def set_binary(self, value):
        """True selects BINARY wire format, False selects TEXT. Updates
        both the host's parser and the device, and drains."""
        self._binary = bool(value)
        self.send(f"FORM:DATA {'BIN' if self._binary else 'TEXT'}")
        self.reset_input_buffer()

    def get_binary(self):
        return self._binary

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
                chan = cnt >> _COUNTER_CHAN_SHIFT
                ticks = cnt & _COUNTER_CHAN_MASK
                yield Record("ts", chan, sec, ticks * _NS_PER_TICK, None)
            leftover = buf[n * _BINARY_RECORD_LEN:]

    def _iter_text(self, deadline):
        while True:
            if deadline is not None and time.monotonic() >= deadline:
                return
            line = self._ser.readline().decode(errors="replace").strip()
            if not line:
                continue
            if line.startswith("#"):
                yield Record("comment", None, None, None, line)
                continue
            parts = line.split()
            if len(parts) != 2:
                continue
            try:
                chan = int(parts[0])
                sec_str, _, ns_str = parts[1].partition(".")
                seconds = int(sec_str)
                nanoseconds = int(ns_str) if ns_str else 0
            except ValueError:
                continue
            yield Record("ts", chan, seconds, nanoseconds, None)

    def iter_records(self):
        """Generator yielding Record namedtuples. Enables streaming on
        the device first, then loops forever; the caller breaks out
        when done."""
        self.set_stream_enabled(True)
        return (self._iter_binary(None) if self._binary
                else self._iter_text(None))

    def read_for(self, duration_s):
        """Like iter_records, but returns after duration_s seconds of
        wall clock even if the device is silent."""
        self.set_stream_enabled(True)
        self._ser.timeout = 0.05
        deadline = time.monotonic() + duration_s
        return (self._iter_binary(deadline) if self._binary
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


def cmd_format(ts, args):
    if args.value is None:
        print("BIN" if ts.get_binary() else "TEXT")
    else:
        ts.set_binary(args.value.lower() in ("bin", "binary"))


def cmd_raw(ts, args):
    cmd = args.command
    if "?" in cmd:
        print(ts.query(cmd))
    else:
        ts.send(cmd)


def cmd_stream(ts, args):
    ts.set_binary(args.format == "binary")
    try:
        for r in ts.iter_records():
            if r.kind == "ts":
                # Reproduce the device's TEXT format exactly from the
                # integer fields -- no float round-trip, so it is
                # byte-for-byte what the device would have emitted.
                sys.stdout.write(
                    f"{r.channel} {r.seconds}.{r.nanoseconds:09d}\n")
            else:
                sys.stdout.write(r.comment + "\n")
            sys.stdout.flush()
    except KeyboardInterrupt:
        pass
    finally:
        # `stream` switches to BIN for performance, then decodes back
        # to ASCII for stdout. Restore TEXT on the way out so other
        # tools that come along see the documented default.
        ts.set_binary(False)


def main():
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default=None,
                   help="Serial port (default: autodetect via USB VID/PID + "
                        "*IDN? probe)")
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("idn", help="Read *IDN? identification string")
    sp.set_defaults(func=cmd_idn)

    sp = sub.add_parser("reset", help="Send *RST (defaults, streaming on)")
    sp.set_defaults(func=cmd_reset)

    sp = sub.add_parser("slope", help="Set or query input slope")
    sp.add_argument("channel", type=int, choices=[0, 1, 2, 3])
    sp.add_argument("value", nargs="?", choices=["pos", "neg", "both",
                                                 "POS", "NEG", "BOTH"],
                    help="Omit to query")
    sp.set_defaults(func=cmd_slope)

    sp = sub.add_parser("div", help="Set or query channel divider (>= 1)")
    sp.add_argument("channel", type=int, choices=[0, 1, 2, 3])
    sp.add_argument("n", nargs="?", type=int, help="Omit to query")
    sp.set_defaults(func=cmd_div)

    sp = sub.add_parser("format", help="Set or query the wire format "
                                       "(persists after tsctl exits)")
    sp.add_argument("value", nargs="?",
                    choices=["text", "binary", "bin", "TEXT", "BINARY", "BIN"],
                    help="Omit to query")
    sp.set_defaults(func=cmd_format)

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
    with Timestamper(args.port) as ts:
        args.func(ts, args)


if __name__ == "__main__":
    main()
