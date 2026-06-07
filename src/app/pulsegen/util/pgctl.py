#!/usr/bin/env python3

"""Configure a Lectrobox Pulsegen over its USB CDC SCPI interface.

CLI usage examples:
  pgctl.py idn
  pgctl.py state 0 ON
  pgctl.py period 0 1e-6
  pgctl.py width 0 100e-9
  pgctl.py count 0 100
  pgctl.py burst-period 0 1e-3
  pgctl.py pulse 0 1e-6 100e-9       # period + width + state ON in one shot
  pgctl.py freq 0 1000 50            # 1 kHz, 50% duty
  pgctl.py off 0                     # state OFF
  pgctl.py off                       # state OFF on every channel
  pgctl.py reset
  pgctl.py error
  pgctl.py raw '*IDN?'
  pgctl.py --port /dev/ttyACM2 state 2 ON

As a library, open a Pulsegen instance and call methods on it. The
device's SCPI surface is setters-only (no per-parameter queries), so
the API mirrors that.

  from pgctl import Pulsegen

  # Autodetects by USB VID/PID (pass port="/dev/ttyACM1" to override).
  with Pulsegen() as pg:
      print(pg.idn())                          # "Lectrobox,Pulsegen,..."
      pg.reset()                               # *RST -- all outputs off

      # Per-channel configuration (channel in 0..3). Periods and widths
      # are in seconds; the device works in picosecond units internally
      # and accepts scientific notation (e.g. 1.5e-6 = 1.5 microseconds).
      pg.set_period(0, 1e-6)                   # 1 microsecond period
      pg.set_width(0, 100e-9)                  # 100 nanosecond pulse width
      pg.set_state(0, True)                    # OUTP0:STAT ON

      # Convenience: one call to configure-and-start.
      pg.pulse(1, period_s=2e-7, width_s=5e-8)

      # Burst mode: run N pulses each BURSt:PERiod seconds.
      pg.set_count(2, 100)                     # 100 pulses per burst
      pg.set_burst_period(2, 1e-3)             # bursts repeat every 1 ms
      pg.set_state(2, True)

      # Latched-error access for diagnostics.
      pg.clear_errors()                        # *CLS
      print(pg.get_error())                    # '0,"No error"'

      # Escape hatches for raw SCPI.
      pg.send("OUTP3:STAT OFF")                # no response read
      print(pg.query("*IDN?"))                 # returns one line
"""

import argparse
import serial
import serial.tools.list_ports
import sys
import time


PULSEGEN_VID = 0x1209  # pid.codes
PULSEGEN_PID = 0x71C5  # Lectrobox Pulsegen
IDN_PREFIX = "Lectrobox,Pulsegen"
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

    def set_state(self, channel, on):
        self.send(f"OUTP{channel}:STAT {'ON' if on else 'OFF'}")

    def set_period(self, channel, seconds):
        """Pulse period in seconds. Resolution down to 250 ps in HRTIM
        mode; falls back to TIM3 (8 ns base tick, up to ~34 s) for
        longer periods. Errors latch in SYST:ERR if the period is out
        of range or width >= period."""
        self.send(f"SOUR{channel}:PULS:PER {seconds:g}")

    def set_width(self, channel, seconds):
        """Pulse high-time width in seconds. Must be strictly less than
        the period."""
        self.send(f"SOUR{channel}:PULS:WIDT {seconds:g}")

    def set_count(self, channel, n):
        """Number of pulses per burst. 0 = continuous (no burst gating)."""
        self.send(f"SOUR{channel}:PULS:COUN {int(n)}")

    def set_burst_period(self, channel, seconds):
        """Period between bursts. 0 with COUN>0 = single shot."""
        self.send(f"SOUR{channel}:BURS:PER {seconds:g}")

    # ---- High-level helpers -----------------------------------------------

    def pulse(self, channel, period_s, width_s):
        """Configure period + width and start the channel in one call."""
        self.set_period(channel, period_s)
        self.set_width(channel, width_s)
        self.set_state(channel, True)

    def freq(self, channel, hz, duty=0.5):
        """Drive `channel` at `hz` Hz with the given duty cycle (0..1)."""
        if not 0 < duty < 1:
            raise ValueError("duty must be between 0 and 1 (exclusive)")
        period_s = 1.0 / hz
        self.pulse(channel, period_s, period_s * duty)

    def off(self, channel=None):
        """Turn off `channel`, or every channel if `channel` is None."""
        if channel is None:
            for ch in range(NUM_CHANNELS):
                self.set_state(ch, False)
        else:
            self.set_state(channel, False)


# ---- CLI ------------------------------------------------------------------

def cmd_idn(pg, args):
    print(pg.idn())


def cmd_reset(pg, args):
    pg.reset()


def cmd_state(pg, args):
    on = args.value.upper() in ("ON", "1", "TRUE")
    pg.set_state(args.channel, on)


def cmd_period(pg, args):
    pg.set_period(args.channel, args.seconds)


def cmd_width(pg, args):
    pg.set_width(args.channel, args.seconds)


def cmd_count(pg, args):
    pg.set_count(args.channel, args.n)


def cmd_burst_period(pg, args):
    pg.set_burst_period(args.channel, args.seconds)


def cmd_pulse(pg, args):
    pg.pulse(args.channel, args.period, args.width)


def cmd_freq(pg, args):
    pg.freq(args.channel, args.hz, args.duty / 100.0)


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

    sp = sub.add_parser("reset", help="Send *RST (disables all outputs)")
    sp.set_defaults(func=cmd_reset)

    sp = sub.add_parser("state", help="Enable or disable a channel output")
    sp.add_argument("channel", type=int, choices=range(NUM_CHANNELS))
    sp.add_argument("value", choices=["on", "off", "ON", "OFF", "1", "0"])
    sp.set_defaults(func=cmd_state)

    sp = sub.add_parser("period", help="Set pulse period in seconds")
    sp.add_argument("channel", type=int, choices=range(NUM_CHANNELS))
    sp.add_argument("seconds", type=float)
    sp.set_defaults(func=cmd_period)

    sp = sub.add_parser("width", help="Set pulse width in seconds")
    sp.add_argument("channel", type=int, choices=range(NUM_CHANNELS))
    sp.add_argument("seconds", type=float)
    sp.set_defaults(func=cmd_width)

    sp = sub.add_parser("count", help="Set pulse count per burst (0 = continuous)")
    sp.add_argument("channel", type=int, choices=range(NUM_CHANNELS))
    sp.add_argument("n", type=int)
    sp.set_defaults(func=cmd_count)

    sp = sub.add_parser("burst-period",
                        help="Set burst-repeat period in seconds (0 = single shot)")
    sp.add_argument("channel", type=int, choices=range(NUM_CHANNELS))
    sp.add_argument("seconds", type=float)
    sp.set_defaults(func=cmd_burst_period)

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
