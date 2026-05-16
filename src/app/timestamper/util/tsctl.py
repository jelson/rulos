#!/usr/bin/env python3

"""Configure a LectroTIC-4 over its USB CDC SCPI interface.

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

As a library, open a LectroTIC4 instance and call methods on it.
Records are always transferred in the device's 8-byte binary wire
format and decoded for you; query()/get_*() briefly silence the
stream so SCPI responses come back clean.

  from tsctl import LectroTIC4

  # Autodetects by USB VID/PID (pass port="/dev/ttyACM1" to override).
  with LectroTIC4() as tic:
      print(tic.idn())                         # "Lectrobox,LectroTIC-4,..."
      tic.reset()                              # *RST

      # Per-channel input configuration (channel ∈ 0..3).
      tic.set_slope(0, "POS")                  # rising edges only
      tic.set_slope(1, "NEG")                  # falling edges only
      tic.set_slope(2, "BOTH")                 # every transition
      tic.set_divider(0, 100)                  # report every 100th edge

      # Read configuration back.
      print(tic.get_slope(0))                  # "POS"
      print(tic.get_divider(0))                # "100"

      # Capture for 5 seconds (loops forever via iter_records()).
      for r in tic.read_for(5.0):
          print(f"ch {r.channel}: t = {r.seconds}.{r.nanoseconds:09d} s")

      # Latched-error access for diagnostics.
      tic.clear_errors()                       # *CLS
      print(tic.get_error())                   # "0,\"No error\""

      # Escape hatches for raw SCPI.
      tic.send("FOO:BAR 1")                    # no response read
      print(tic.query("FOO:BAR?"))             # returns one line
"""

import argparse
from collections import namedtuple
import os
import serial
import serial.tools.list_ports
import sys
import time

# Yielded by LectroTIC4.iter_records / read_for: one captured edge.
# `seconds` and `nanoseconds` are integers -- whole seconds since the
# device booted plus the 0..999_999_999 ns within that second -- kept
# separate on purpose: a single 64-bit float can't represent enough
# distinct nanoseconds to survive runs past ~104 days. Use
# record_seconds() for relative math over short windows where a float
# is fine.
Record = namedtuple("Record", "channel seconds nanoseconds")


def record_seconds(r):
    """Float seconds-since-boot for a 'ts' Record. Convenient for
    relative measurements over short windows (gaps, burst durations).
    Do NOT use it to store or compare absolute timestamps from runs
    longer than ~104 days: that is exactly the 2**53-nanosecond
    float64 limit r.seconds/r.nanoseconds exist to avoid."""
    return r.seconds + r.nanoseconds * 1e-9

TIMESTAMPER_VID = 0x1209  # pid.codes
TIMESTAMPER_PID = 0x71C4  # LectroTIC-4
IDN_PREFIX = "Lectrobox,LectroTIC-4"

_BINARY_RECORD_LEN = 8
_NS_PER_TICK = 4
_COUNTER_CHAN_SHIFT = 30
_COUNTER_CHAN_MASK = 0x3FFFFFFF


def autodetect_port():
    """Return the device path of the (single) attached LectroTIC-4."""
    candidates = [p.device for p in serial.tools.list_ports.comports()
                  if p.vid == TIMESTAMPER_VID and p.pid == TIMESTAMPER_PID]
    if not candidates:
        sys.exit(f"No LectroTIC-4 found (VID:PID "
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
        sys.exit("Found device(s) at the LectroTIC-4 VID:PID but none "
                 "answered *IDN? as a LectroTIC-4.")
    if len(matches) > 1:
        sys.exit(f"Multiple LectroTIC-4s found: {matches}. Use --port.")
    return matches[0]


def _drain_raw(ser, quiet_s=0.1, max_s=1.0):
    """Read until the line goes quiet for quiet_s, or max_s elapses.
    Standalone helper used by autodetect_port before a LectroTIC4
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


class LectroTIC4:
    """A connected LectroTIC-4. Pass port=... to override autodetect.
    Use as a context manager (preferred) or call close() explicitly.

    Connecting does not change device state. The library transfers
    records only in the binary wire format: a streaming read puts the
    device in BIN, and close() restores whatever format it was in
    before the first stream. A handle that never streams never touches
    the format. The `format` command is a passthrough for
    setting/querying the device's FORM:DATA (for other tools that read
    the port) and persists."""

    # Device wire format captured before the first streaming read, so
    # close() can restore it. None until a stream has been started.
    _restore_format = None

    def __init__(self, port=None, timeout=0.5):
        self._ser = serial.Serial(port or autodetect_port(), timeout=timeout)
        # The library's contract is that streaming is on at entry and
        # at exit. query() briefly disables the stream so a response
        # comes back clean; _stream_on tracks that so query() can skip
        # the toggle when a caller has deliberately silenced the stream.
        self._stream_on = True
        self.send("OUTP:STAT ON")
        self.reset_input_buffer()

    # ---- Lifecycle ----------------------------------------------------

    @property
    def port(self):
        return self._ser.port

    @property
    def usb_serial(self):
        """The USB iSerialNumber the OS sees for this device (the
        STM32 96-bit unique ID as hex); should equal the serial field
        of *IDN?. None if it can't be read.

        We already know our port; this is only a metadata lookup for
        it. pyserial surfaces USB descriptor fields solely through the
        port list (no handle-/path-keyed accessor), so we pick our
        known port's row out of comports()."""
        return next((p.serial_number
                     for p in serial.tools.list_ports.comports()
                     if p.device == self._ser.port), None)

    def close(self):
        """Restore streaming to ON (the library's documented default).
        If this handle ever streamed, also put the wire format back to
        what the device had before the first stream. Slope and divider
        are left exactly as the caller set them. Then release the port."""
        try:
            if not self._stream_on:
                self.set_stream_enabled(True)
            if self._restore_format is not None:
                self.send(f"FORM:DATA {self._restore_format}")
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
        """*RST: the device returns to its defaults (TEXT wire format,
        streaming on). Sync the streaming-state cache to match."""
        self.send("*RST")
        self._stream_on = True

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

    # ---- Streaming (binary wire format only) --------------------------

    def _begin_stream(self):
        # Records are consumed only in the 8-byte binary wire format,
        # so put the device in BIN and enable streaming before reading.
        # Capture the format it was in on the first stream so close()
        # can restore it.
        if self._restore_format is None:
            fmt = self.query("FORM:DATA?").strip().upper()
            self._restore_format = "BIN" if fmt.startswith("BIN") else "TEXT"
        self.send("FORM:DATA BIN")
        self.reset_input_buffer()
        self.set_stream_enabled(True)

    def _records(self, deadline):
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
                yield Record(chan, sec, ticks * _NS_PER_TICK)
            leftover = buf[n * _BINARY_RECORD_LEN:]

    def iter_records(self):
        """Yield Record namedtuples forever; the caller breaks out."""
        self._begin_stream()
        return self._records(None)

    def read_for(self, duration_s):
        """Like iter_records, but stop after duration_s wall-clock
        seconds even if the device is silent."""
        self._begin_stream()
        self._ser.timeout = 0.05
        return self._records(time.monotonic() + duration_s)


# ---- CLI ------------------------------------------------------------------

def cmd_idn(tic, args):
    print(tic.idn())


def cmd_reset(tic, args):
    tic.reset()


def cmd_slope(tic, args):
    if args.value is None:
        print(tic.get_slope(args.channel))
    else:
        tic.set_slope(args.channel, args.value)


def cmd_div(tic, args):
    if args.n is None:
        print(tic.get_divider(args.channel))
    else:
        tic.set_divider(args.channel, args.n)


def cmd_format(tic, args):
    if args.value is None:
        print(tic.query("FORM:DATA?").strip())
    else:
        fmt = "BIN" if args.value.lower() in ("bin", "binary") else "TEXT"
        tic.send(f"FORM:DATA {fmt}")


def cmd_raw(tic, args):
    cmd = args.command
    if "?" in cmd:
        print(tic.query(cmd))
    else:
        tic.send(cmd)


def cmd_stream(tic, args):
    # Print each captured record as a plain ASCII line until ^C or the
    # downstream pipe closes.
    try:
        for r in tic.iter_records():
            sys.stdout.write(
                f"{r.channel} {r.seconds}.{r.nanoseconds:09d}\n")
            sys.stdout.flush()
    except KeyboardInterrupt:
        pass
    except BrokenPipeError:
        # Downstream closed the pipe (e.g. `tsctl stream | head`).
        # Redirect stdout to /dev/null so the interpreter's final
        # flush doesn't raise again.
        os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())


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
                        help="Forward timestamps to stdout until ^C "
                             "(binary on the wire for throughput, printed "
                             "as ASCII; device format restored on exit)")
    sp.set_defaults(func=cmd_stream)

    args = p.parse_args()
    with LectroTIC4(args.port) as tic:
        args.func(tic, args)


if __name__ == "__main__":
    main()
