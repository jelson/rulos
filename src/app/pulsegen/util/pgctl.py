#!/usr/bin/env python3

"""Configure a Lectrobox Pulsegen over its USB CDC SCPI interface.

The device has two modes:
  ASYNC (default): four fully independent channels, each with its own
    period, width, and rising-edge delay.
  SYNC: all four channels share one period, phase-locked, so per-channel
    delays place rising edges relative to a common t=0.

Each channel spans ~24 ns to ~34 s. The instrument places fine edges with
a high-resolution timer below ~524 us (250 ps grid) and hands off to a
general-purpose timer above it (8 ns grid); the handoff is invisible --
width, delay, sync, and burst behave the same on both sides. Setting a
channel's period sets just that channel in async, or all four in sync.
Width and delay are always per channel.

A domain can also run in burst mode: exactly N pulses (1..63488), then
quiet, repeating at a programmable interval. The pulse count and
intra-burst spacing are hardware-exact; the repetition interval is
software-scheduled on the device (~10 ms granularity). One domain bursts
at a time in async; sync mode bursts all four channels coherently.

CLI usage examples:
  pgctl.py idn
  pgctl.py mode sync
  pgctl.py mode                      # query
  pgctl.py state 0 ON
  pgctl.py period 0 1e-6
  pgctl.py width 0 100e-9
  pgctl.py delay 1 250e-12
  pgctl.py pulse 0 1e-6 100e-9       # period + width + state ON in one shot
  pgctl.py freq 0 1e6 50             # 1 MHz, 50% duty
  pgctl.py stair 1e-6 100e-9 250e-12 # sync 4-channel 250 ps stair
  pgctl.py burst 0 1e-6 100e-9 50    # 50-pulse bursts, repeating 1/sec
  pgctl.py burst 0 1e-6 100e-9 50 0.2  # ... every 200 ms
  pgctl.py off 0                     # state OFF
  pgctl.py off                       # state OFF on every channel
  pgctl.py reset
  pgctl.py error
  pgctl.py raw '*IDN?'
  pgctl.py --port /dev/ttyACM2 state 2 ON

As a library, open a Pulsegen instance and call methods on it.

  from pgctl import Pulsegen

  # Autodetects by USB VID/PID (pass port="/dev/ttyACM1" to override).
  with Pulsegen() as pg:
      print(pg.idn())                          # "Lectrobox,PG-4,..."
      pg.reset()                               # *RST -- ASYNC, all off

      # Periods/widths/delays are in seconds; the device works in
      # picoseconds internally and accepts scientific notation.
      pg.set_period(0, 1e-6)                   # 1 us period (ch0 only)
      pg.set_width(0, 100e-9)                  # 100 ns width
      pg.set_delay(0, 0)                       # rising-edge offset
      pg.set_state(0, True)                    # OUTP0:STAT ON

      # Convenience: configure-and-start.
      pg.pulse(1, period_s=2e-7, width_s=5e-8)

      # N-cycle bursts: 50 pulses at 1 us spacing, repeating once/sec.
      pg.burst(0, period_s=1e-6, width_s=100e-9, ncyc=50)

      # Sync mode + a 4-channel 250 ps stair for timestamper testing.
      pg.stair(period_s=1e-6, width_s=100e-9, step_s=250e-12)

      # Latched-error access for diagnostics.
      pg.clear_errors()                        # *CLS
      print(pg.get_error())                    # '0,"No error"'

      # Escape hatches for raw SCPI.
      pg.send("OUTP3:STAT OFF")                # no response read
      print(pg.query("MODE?"))                 # returns one line
"""

import argparse
import enum
import serial
import serial.tools.list_ports
import sys


PULSEGEN_VID = 0x1209  # pid.codes
PULSEGEN_PID = 0x71C5  # Lectrobox Pulsegen
IDN_PREFIX = "Lectrobox,PG-4"
NUM_CHANNELS = 4


def autodetect_port():
    """Return the device path of the (single) attached Pulsegen."""
    candidates = [p.device for p in serial.tools.list_ports.comports()
                  if p.vid == PULSEGEN_VID and p.pid == PULSEGEN_PID]
    if not candidates:
        sys.exit(f"No Pulsegen found (VID:PID "
                 f"{PULSEGEN_VID:04x}:{PULSEGEN_PID:04x}). "
                 f"Try --port.")
    matches = []
    for dev in candidates:
        try:
            ser = serial.Serial(dev, timeout=0.5)
        except serial.SerialException as e:
            print(f"  {dev}: {e}", file=sys.stderr)
            continue
        try:
            ser.write(b"*IDN?\n")
            ser.flush()
            idn = ser.readline().decode(errors="replace").strip()
            if idn.startswith(IDN_PREFIX):
                matches.append(dev)
        finally:
            ser.close()
    if not matches:
        sys.exit("Found device(s) at the Pulsegen VID:PID but none "
                 "answered *IDN? as a Pulsegen.")
    if len(matches) > 1:
        sys.exit(f"Multiple Pulsegens found: {matches}. Use --port.")
    return matches[0]


class Pulsegen:
    """A connected Pulsegen. Pass port=... to override autodetect.
    Use as a context manager (preferred) or call close() explicitly.

    Connecting does not change device state. close() does not modify
    any output configuration -- whatever channels were running stay
    running."""

    class Mode(enum.Enum):
        """Channel timing mode for set_mode(). The value is the SCPI
        keyword sent to the device."""
        ASYNC = "ASYNC"  # four independent channels
        SYNC = "SYNC"    # all four channels share one phase-locked period

    # Expose the members on the class so callers can write Pulsegen.SYNC.
    ASYNC = Mode.ASYNC
    SYNC = Mode.SYNC

    def __init__(self, port=None, timeout=0.5):
        self._ser = serial.Serial(port or autodetect_port(), timeout=timeout)
        self._ser.reset_input_buffer()

    @property
    def port(self):
        return self._ser.port

    @property
    def usb_serial(self):
        """The USB iSerialNumber for this device (USBD_SERIAL_PREFIX +
        STM32 96-bit unique ID as hex); equals the serial field of *IDN?.
        None if it can't be read."""
        return next((p.serial_number
                     for p in serial.tools.list_ports.comports()
                     if p.device == self._ser.port), None)

    def close(self):
        self._ser.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # ---- Low-level SCPI I/O -----------------------------------------------

    def send(self, cmd):
        """Send a single SCPI command (newline appended)."""
        self._ser.write((cmd + "\n").encode())
        self._ser.flush()

    def query(self, cmd):
        """Send a query and read one line of response."""
        self._ser.reset_input_buffer()
        self.send(cmd)
        self._ser.timeout = 0.5
        return self._ser.readline().decode(errors="replace").strip()

    # ---- SCPI conveniences ------------------------------------------------

    def idn(self):
        return self.query("*IDN?")

    def reset(self):
        """*RST: disable all outputs and clear all per-channel config."""
        self.send("*RST")

    def clear_errors(self):
        self.send("*CLS")

    def get_error(self):
        return self.query("SYST:ERR?")

    def set_mode(self, mode):
        """Select Pulsegen.SYNC (all channels one phase-locked period) or
        Pulsegen.ASYNC (four independent channels)."""
        if not isinstance(mode, self.Mode):
            raise TypeError(
                f"mode must be Pulsegen.SYNC or Pulsegen.ASYNC, got {mode!r}")
        self.send(f"MODE {mode.value}")

    def get_mode(self):
        """Returns 'ASYNC' or 'SYNC'."""
        return self.query("MODE?")

    def set_state(self, channel, on):
        self.send(f"OUTP{channel}:STAT {'ON' if on else 'OFF'}")

    def set_period(self, channel, seconds):
        """Pulse period in seconds for this channel's domain (just this
        channel in async mode, all four channels in sync mode). Range is
        ~24 ns to ~34 s. Errors latch in SYST:ERR if out of range or
        width >= period."""
        self.send(f"SOUR{channel}:PULS:PER {seconds:.12g}")

    def set_width(self, channel, seconds):
        """Pulse high-time width in seconds (per channel). Must be
        strictly less than the period."""
        self.send(f"SOUR{channel}:PULS:WIDT {seconds:.12g}")

    def set_delay(self, channel, seconds):
        """Rising-edge offset in seconds (per channel); observable in
        SYNC mode (async channels free-run with no shared t=0). Placed on
        the 250 ps grid in the fast regime, 8 ns in the slow regime. delay
        + width must not exceed the period."""
        self.send(f"SOUR{channel}:PULS:DEL {seconds:.12g}")

    def set_burst_state(self, channel, on):
        """Enable or disable burst mode on this channel's domain. With
        burst on, the domain emits exactly ncycles pulses, rests low,
        and repeats every burst period."""
        self.send(f"SOUR{channel}:BURS:STAT {'ON' if on else 'OFF'}")

    def get_burst_state(self, channel):
        """True iff burst mode is enabled on this channel's domain."""
        return self.query(f"SOUR{channel}:BURS:STAT?") == "ON"

    def set_burst_ncycles(self, channel, ncyc):
        """Pulses per burst (1..63488), hardware-counted."""
        self.send(f"SOUR{channel}:BURS:NCYC {ncyc}")

    def get_burst_ncycles(self, channel):
        return int(self.query(f"SOUR{channel}:BURS:NCYC?"))

    def set_burst_period(self, channel, seconds):
        """Burst repetition interval in seconds (default 1.0). Software-
        scheduled on the device (~10 ms granularity); must exceed the
        burst length by at least 50 ms, max 60 s."""
        self.send(f"SOUR{channel}:BURS:INT:PER {seconds:.12g}")

    def get_burst_period(self, channel):
        return float(self.query(f"SOUR{channel}:BURS:INT:PER?"))

    # ---- High-level helpers -----------------------------------------------

    def pulse(self, channel, period_s, width_s, delay_s=0.0):
        """Configure period + width (+ optional delay) and start the
        channel as a continuous train in one call. Clears any leftover
        burst state on the domain first."""
        self.set_burst_state(channel, False)
        self.set_period(channel, period_s)
        self.set_width(channel, width_s)
        self.set_delay(channel, delay_s)
        self.set_state(channel, True)

    def burst(self, channel, period_s, width_s, ncyc, rep_s=1.0,
              delay_s=0.0):
        """Configure and start N-cycle bursts: ncyc pulses at period_s
        spacing and width_s width, repeating every rep_s seconds."""
        self.set_period(channel, period_s)
        self.set_width(channel, width_s)
        self.set_delay(channel, delay_s)
        self.set_burst_ncycles(channel, ncyc)
        self.set_burst_period(channel, rep_s)
        self.set_burst_state(channel, True)
        self.set_state(channel, True)

    def freq(self, channel, hz, duty=0.5):
        """Drive `channel` at `hz` Hz with the given duty cycle (0..1)."""
        if not 0 < duty < 1:
            raise ValueError("duty must be between 0 and 1 (exclusive)")
        period_s = 1.0 / hz
        self.pulse(channel, period_s, period_s * duty)

    def stair(self, period_s, width_s, step_s):
        """Sync mode 4-channel staircase: every channel at `period_s`
        and `width_s`, with channel n's rising edge delayed by n*step_s.
        The canonical timestamper edge-resolution test."""
        self.set_mode(self.SYNC)
        # Sync mode is a single domain, so one channel clears burst
        # state for all four.
        self.set_burst_state(0, False)
        for ch in range(NUM_CHANNELS):
            self.set_period(ch, period_s)
            self.set_width(ch, width_s)
            self.set_delay(ch, ch * step_s)
            self.set_state(ch, True)

    def off(self, channel=None):
        """Turn off `channel`, or every channel if `channel` is None."""
        if channel is None:
            for ch in range(NUM_CHANNELS):
                self.set_state(ch, False)
        else:
            self.set_state(channel, False)

    # ---- Rate / burst conveniences (the timestamper test surface) ---------
    #
    # Continuous output spans the full ~24 ns to ~34 s range (the device hands
    # HRTIM off to a GP timer above ~524 us transparently). BURST, however, is
    # fast-regime only -- the spacing must land in the HRTIM range -- so
    # pulse_burst() quantizes to the 250 ps grid and rejects spacings > ~524 us.

    MIN_HZ = 0.03              # continuous floor (~34 s period)
    MIN_BURST_SPACING_NS = 48  # tightest spacing offered: leaves a clean
                               # half-period each way above the HRTIM
                               # minimum-compare limit
    _TICK_PS = 250
    _PER_MAX = 0xFFDF
    _MIN_PER = (0x60, 0x30, 0x18, 0xC, 0x6, 0x3)

    @classmethod
    def quantize_period_ps(cls, period_ps):
        """The period (ps) the HRTIM actually produces for a requested
        period, or None if it is outside the ~24 ns .. ~524 us range.
        Mirrors select_ckpsc in pulsegen.c: the tick is 250 ps times a
        power-of-two prescaler, and the period register PER =
        floor(period / tick) must be within [min_per, PER_MAX]."""
        for k in range(6):
            tick = cls._TICK_PS << k
            per = period_ps // tick
            if cls._MIN_PER[k] <= per <= cls._PER_MAX:
                return per * tick
        return None

    def _check(self, what):
        err = self.get_error()
        if not err.startswith("0,"):
            raise RuntimeError(f"PG-4 rejected {what}: {err}")

    def periodic(self, channel, hz, width_s=None):
        """Drive `channel` with a continuous pulse train at `hz` Hz.
        Width defaults to min(5 us, period/4). Returns the actual rate
        (the PG quantizes to its 250 ps grid -- sub-ppb of any in-range
        rate, and coherent with a shared reference). Raises if `hz` is
        below the ~0.03 Hz floor (34 s period)."""
        if hz < self.MIN_HZ:
            raise ValueError(f"{hz:g} Hz is below the PG-4's "
                             f"~{self.MIN_HZ:g} Hz floor")
        period_s = 1.0 / hz
        if width_s is None:
            width_s = min(5e-6, period_s / 4)
        self.pulse(channel, period_s, width_s)
        self._check(f"{hz:g} Hz x {width_s:g} s")
        return hz

    def pulse_burst(self, channel, spacing_ns, ncyc, width_s=None,
                    rep_s=1.0):
        """Drive `channel` with `ncyc` pulses `spacing_ns` apart,
        repeating every `rep_s`. The spacing is clamped up to the PG-4's
        ~48 ns floor and quantized to the HRTIM grid; the actual spacing
        (ns) is returned. Width defaults to min(50 ns, half the
        spacing)."""
        spacing_ns = max(spacing_ns, self.MIN_BURST_SPACING_NS)
        actual_ps = self.quantize_period_ps(round(spacing_ns * 1000))
        if actual_ps is None:
            raise ValueError(f"{spacing_ns:g} ns spacing is beyond the "
                             f"PG-4's ~524 us period ceiling")
        if width_s is None:
            width_s = min(50e-9, actual_ps * 1e-12 / 2)
        self.burst(channel, actual_ps * 1e-12, width_s, ncyc, rep_s=rep_s)
        self._check(f"{ncyc}-pulse burst at {spacing_ns:g} ns spacing")
        return actual_ps / 1000.0


# ---- CLI ------------------------------------------------------------------

def _report(pg):
    """Read back the latched error in the same connection a setter just used
    and surface it. The device clears its error latch when the port is
    reopened, so a separate `pgctl.py error` invocation can't see it -- check
    here, while still connected."""
    err = pg.get_error()
    if not err.startswith('0,'):
        sys.exit(err)


def cmd_idn(pg, args):
    print(pg.idn())


def cmd_reset(pg, args):
    pg.reset()


def cmd_mode(pg, args):
    if args.value is None:
        print(pg.get_mode())
    else:
        pg.set_mode(Pulsegen.Mode(args.value.upper()))


def cmd_state(pg, args):
    on = args.value.upper() in ("ON", "1", "TRUE")
    pg.set_state(args.channel, on)
    _report(pg)


def cmd_period(pg, args):
    pg.set_period(args.channel, args.seconds)
    _report(pg)


def cmd_width(pg, args):
    pg.set_width(args.channel, args.seconds)
    _report(pg)


def cmd_delay(pg, args):
    pg.set_delay(args.channel, args.seconds)
    _report(pg)


def cmd_pulse(pg, args):
    pg.pulse(args.channel, args.period, args.width)
    _report(pg)


def cmd_freq(pg, args):
    pg.freq(args.channel, args.hz, args.duty / 100.0)
    _report(pg)


def cmd_stair(pg, args):
    pg.stair(args.period, args.width, args.step)
    _report(pg)


def cmd_burst(pg, args):
    pg.burst(args.channel, args.period, args.width, args.ncyc, args.rep)
    _report(pg)


def cmd_off(pg, args):
    pg.off(args.channel)


def cmd_error(pg, args):
    print(pg.get_error())


def cmd_raw(pg, args):
    cmd = args.command
    if "?" in cmd:
        print(pg.query(cmd))
    else:
        pg.send(cmd)


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

    sp = sub.add_parser("reset", help="Send *RST (ASYNC, all outputs off)")
    sp.set_defaults(func=cmd_reset)

    sp = sub.add_parser("mode", help="Set or query ASYNC/SYNC mode")
    sp.add_argument("value", nargs="?",
                    choices=["async", "sync", "ASYNC", "SYNC"],
                    help="Omit to query")
    sp.set_defaults(func=cmd_mode)

    sp = sub.add_parser("state", help="Enable or disable a channel output")
    sp.add_argument("channel", type=int, choices=range(NUM_CHANNELS))
    sp.add_argument("value", choices=["on", "off", "ON", "OFF", "1", "0"])
    sp.set_defaults(func=cmd_state)

    sp = sub.add_parser("period",
                        help="Set period in seconds (this channel async, all sync)")
    sp.add_argument("channel", type=int, choices=range(NUM_CHANNELS))
    sp.add_argument("seconds", type=float)
    sp.set_defaults(func=cmd_period)

    sp = sub.add_parser("width", help="Set pulse width in seconds (per channel)")
    sp.add_argument("channel", type=int, choices=range(NUM_CHANNELS))
    sp.add_argument("seconds", type=float)
    sp.set_defaults(func=cmd_width)

    sp = sub.add_parser("delay",
                        help="Set rising-edge offset in seconds (per channel)")
    sp.add_argument("channel", type=int, choices=range(NUM_CHANNELS))
    sp.add_argument("seconds", type=float)
    sp.set_defaults(func=cmd_delay)

    sp = sub.add_parser("pulse",
                        help="Configure period + width and turn the channel on")
    sp.add_argument("channel", type=int, choices=range(NUM_CHANNELS))
    sp.add_argument("period", type=float, help="Period in seconds")
    sp.add_argument("width", type=float, help="Width in seconds")
    sp.set_defaults(func=cmd_pulse)

    sp = sub.add_parser("freq",
                        help="Drive a channel at a given frequency and duty cycle")
    sp.add_argument("channel", type=int, choices=range(NUM_CHANNELS))
    sp.add_argument("hz", type=float, help="Frequency in Hz")
    sp.add_argument("duty", nargs="?", type=float, default=50.0,
                    help="Duty cycle in percent (default: 50)")
    sp.set_defaults(func=cmd_freq)

    sp = sub.add_parser("stair",
                        help="SYNC 4-channel staircase: ch n delayed by n*step")
    sp.add_argument("period", type=float, help="Period in seconds")
    sp.add_argument("width", type=float, help="Width in seconds")
    sp.add_argument("step", type=float, help="Per-channel delay step in seconds")
    sp.set_defaults(func=cmd_stair)

    sp = sub.add_parser("burst",
                        help="N-cycle bursts: configure and start")
    sp.add_argument("channel", type=int, choices=range(NUM_CHANNELS))
    sp.add_argument("period", type=float, help="Pulse spacing in seconds")
    sp.add_argument("width", type=float, help="Width in seconds")
    sp.add_argument("ncyc", type=int, help="Pulses per burst (1..63488)")
    sp.add_argument("rep", nargs="?", type=float, default=1.0,
                    help="Burst repetition interval in seconds (default: 1)")
    sp.set_defaults(func=cmd_burst)

    sp = sub.add_parser("off",
                        help="Turn off one channel (or every channel if omitted)")
    sp.add_argument("channel", nargs="?", type=int,
                    choices=range(NUM_CHANNELS))
    sp.set_defaults(func=cmd_off)

    sp = sub.add_parser("error", help="Read the latched SYSTem:ERRor?")
    sp.set_defaults(func=cmd_error)

    sp = sub.add_parser("raw",
                        help="Send a raw SCPI command (queries auto-read)")
    sp.add_argument("command")
    sp.set_defaults(func=cmd_raw)

    args = p.parse_args()
    with Pulsegen(port=args.port) as pg:
        args.func(pg, args)


if __name__ == "__main__":
    main()
