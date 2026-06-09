#!/usr/bin/env python3

"""Pulse-source abstraction for the timestamper test suite.

The tests need a pulse source but not any particular instrument; this
module defines the small PulseGenerator interface they program against
and two implementations:

  RigolDG1022Z -- the lab signal generator, driven over LAN via
      siggen.py. Full capability: periodic trains at any rate down to
      DC, and N-cycle bursts (the basis of the burst / overcapture /
      polarity-burst / single-pulse phases).

  LectroboxPG4 -- a Lectrobox PG-4 channel on USB, driven via
      pgctl.py. Clock-coherent with the timestamper when both share
      the lab 10 MHz reference and happy at much higher rates, but its
      HRTIM timebase tops out at ~524 us periods (no rates below
      ~1.9 kHz). N-cycle bursts are hardware-counted (HRTIM burst mode
      controller, up to 63488 pulses/burst).

Capabilities the tests read:
  can_burst    -- pulse_burst() / isolated single pulses available
  base_hz      -- preferred rate for the slow periodic phases
  base_width_s -- pulse width the periodic phases drive at base_hz
                  (and assert on in both-edges mode)
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                "..", "..", "pulsegen", "util"))


class UnsupportedWaveform(Exception):
    """The generator cannot produce the requested signal."""


class PulseGenerator:
    """What the test suite needs from a pulse source. An
    implementation either produces the requested waveform (returning
    the actual value where hardware quantizes the request) or raises
    UnsupportedWaveform."""

    name = "?"
    can_burst = False
    base_hz = 10.0
    base_width_s = 1e-3

    def idn(self):
        raise NotImplementedError

    def periodic(self, hz, pulse_width_s=None):
        """Continuous pulse train at hz; returns the actual rate.
        Default width is the generator's base_width_s (clamped below
        the period)."""
        raise NotImplementedError

    def pulse_burst(self, spacing_ns, ncyc):
        """ncyc pulses spacing_ns apart, repeating once per second;
        returns the actual spacing in ns."""
        raise UnsupportedWaveform(f"{self.name} has no burst mode")

    def output_off(self):
        raise NotImplementedError

    def close(self):
        raise NotImplementedError

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


class RigolDG1022Z(PulseGenerator):
    """DG1022Z over LAN (see siggen.py)."""

    name = "DG1022Z"
    can_burst = True
    base_hz = 10.0
    base_width_s = 1e-3

    def __init__(self, host="siggen"):
        from siggen import Siggen
        self._sg = Siggen(host=host)

    def idn(self):
        return self._sg.idn()

    def periodic(self, hz, pulse_width_s=None):
        if pulse_width_s is None:
            pulse_width_s = min(self.base_width_s, 0.25 / hz)
        return self._sg.periodic(hz, pulse_width_s=pulse_width_s)

    def pulse_burst(self, spacing_ns, ncyc):
        return self._sg.pulse_burst(spacing_ns, ncyc=ncyc)

    def output_off(self):
        self._sg.output_off()

    def close(self):
        self._sg.close()


class LectroboxPG4(PulseGenerator):
    """One PG-4 output channel over USB CDC (see pgctl.py)."""

    name = "PG-4"
    can_burst = True
    # The HRTIM-only timebase tops out at ~524 us periods, so there
    # are no rates below ~1.9 kHz: the slow phases run at 2 kHz
    # instead of the Rigol's 10 Hz, and the tests' tolerances scale
    # with rate. base_width_s keeps the Rigol's width:period
    # proportion (1 ms : 100 ms).
    MIN_HZ = 1910.0
    base_hz = 2000.0
    base_width_s = 5e-6

    # Mirror of the firmware's period quantization (select_ckpsc in
    # pulsegen.c): the HRTIM tick is 250 ps times a power-of-two
    # prescaler (CKPSC = 0..5); the firmware picks the finest prescaler
    # whose period register value PER = floor(period / tick) is at most
    # PER_MAX and at least that prescaler's minimum (RM0440 "Minimum
    # PER value" table).
    TICK_PS = 250
    PER_MAX = 0xFFDF
    MIN_PER = (0x60, 0x30, 0x18, 0xC, 0x6, 0x3)

    @classmethod
    def _quantize_period_ps(cls, period_ps):
        """The period the PG-4 actually produces for a requested
        period_ps, or None if no prescaler can produce it."""
        for k in range(6):
            tick_ps = cls.TICK_PS << k
            per = period_ps // tick_ps
            if cls.MIN_PER[k] <= per <= cls.PER_MAX:
                return per * tick_ps
        return None

    def __init__(self, port=None, channel=0):
        from pgctl import Pulsegen
        self._pg = Pulsegen(port=port)
        self._ch = channel

    def idn(self):
        return self._pg.idn()

    def _check(self, what):
        err = self._pg.get_error()
        if not err.startswith("0,"):
            raise UnsupportedWaveform(f"PG-4 rejected {what}: {err}")

    def periodic(self, hz, pulse_width_s=None):
        if hz < self.MIN_HZ:
            raise UnsupportedWaveform(
                f"{hz:g} Hz is below the PG-4's ~{self.MIN_HZ:.0f} Hz "
                f"floor (HRTIM period tops out at ~524 us)")
        period_s = 1.0 / hz
        if pulse_width_s is None:
            pulse_width_s = min(self.base_width_s, period_s / 4)
        self._pg.pulse(self._ch, period_s, pulse_width_s)
        self._check(f"{hz:g} Hz x {pulse_width_s:g} s")
        # The PG-4 shares the timestamper's reference and quantizes to
        # its 250 ps grid -- sub-ppb of any rate in range.
        return hz

    # Smallest burst spacing offered: tests probe a generator's floor
    # by requesting tiny spacings and using the clamped actual (the
    # DG1022Z behaves the same way, clamping at ~67 ns). The HRTIM
    # could period down to ~24 ns, but its minimum compare value is
    # also ~24 ns at that prescaler, which would squeeze the pulse's
    # low time below what the timestamper's input network can resolve;
    # 48 ns leaves a clean half-period each way.
    MIN_BURST_SPACING_NS = 48

    def pulse_burst(self, spacing_ns, ncyc):
        spacing_ns = max(spacing_ns, self.MIN_BURST_SPACING_NS)
        period_ps = round(spacing_ns * 1000)
        actual_ps = self._quantize_period_ps(period_ps)
        if actual_ps is None:
            raise UnsupportedWaveform(
                f"{spacing_ns:g} ns spacing is beyond the PG-4's "
                f"~524 us period ceiling")
        # 50 ns width (matching siggen.pulse_burst), shrunk to a half
        # period when the spacing is tighter than 100 ns; 1 s
        # repetition.
        width_s = min(50e-9, actual_ps * 1e-12 / 2)
        self._pg.burst(self._ch, period_ps * 1e-12, width_s, ncyc)
        self._check(f"{ncyc}-pulse burst at {spacing_ns:g} ns spacing")
        return actual_ps / 1000.0

    def output_off(self):
        self._pg.off(self._ch)

    def close(self):
        try:
            self._pg.off(self._ch)
        except Exception:
            pass
        self._pg.close()


def make_generator(kind, siggen_host="siggen", pg_port=None,
                   pg_channel=0):
    if kind == "rigol":
        return RigolDG1022Z(host=siggen_host)
    if kind == "pg4":
        return LectroboxPG4(port=pg_port, channel=pg_channel)
    raise ValueError(f"unknown generator kind {kind!r}")
