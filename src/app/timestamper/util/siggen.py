#!/usr/bin/env python3

"""Control a Rigol DG1022Z signal generator over LAN.

Usage:
  siggen.py periodic <hz>       Continuous pulse at <hz> Hz, 3.3V, 1ms width
  siggen.py 1pps                Shorthand for 'periodic 1'
  siggen.py burst <ns> <ncyc>   Burst of <ncyc> pulses <ns> apart, repeating 1/sec
  siggen.py off                 Turn off channel 1 output
  siggen.py --host HOST         Override default hostname (default: siggen)
"""

import argparse
import socket


class Siggen:
    def __init__(self, host="siggen", port=5555, timeout=5):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(timeout)
        self.sock.connect((host, port))

    def close(self):
        self.sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def cmd(self, scpi_cmd):
        """Send a command and wait for the instrument to finish processing."""
        self.sock.sendall((scpi_cmd + "\n").encode())
        # Use *OPC? to synchronize: it returns "1" only after all
        # pending operations complete, avoiding command pileup.
        self.query("*OPC?")

    def query(self, scpi_cmd):
        self.sock.sendall((scpi_cmd + "\n").encode())
        return self.sock.recv(4096).decode().strip()

    def idn(self):
        return self.query("*IDN?")

    def output_on(self, ch=1):
        self.cmd(f":OUTP{ch} ON")

    def output_off(self, ch=1):
        """Turn off output cleanly: zero amplitude, settle to DC, then off."""
        self.cmd(f":SOUR{ch}:BURS:STAT OFF")
        self.cmd(f":SOUR{ch}:VOLT:HIGH 0")
        self.cmd(f":SOUR{ch}:VOLT:LOW 0")
        self.cmd(f":SOUR{ch}:FUNC DC")
        self.cmd(f":OUTP{ch} OFF")

    def verify(self, queries):
        for q in queries:
            print(f"  {q:30s} {self.query(q)}")

    def periodic(self, hz):
        """Configure continuous pulse output at the given frequency in Hz."""
        self.output_off()
        self.cmd(":SOUR1:BURS:STAT OFF")
        self.cmd(":SOUR1:FUNC PULS")
        self.cmd(f":SOUR1:FREQ {hz}")
        self.cmd(":SOUR1:VOLT:HIGH 3.3")
        self.cmd(":SOUR1:VOLT:LOW 0")
        self.cmd(":SOUR1:PULS:WIDT 0.001")
        self.output_on()

        self.verify([
            ":SOUR1:FUNC?", ":SOUR1:FREQ?", ":SOUR1:VOLT:HIGH?",
            ":SOUR1:VOLT:LOW?", ":SOUR1:PULS:WIDT?", ":SOUR1:BURS:STAT?",
            ":SYST:ERR?",
        ])

    def pulse_burst(self, ns, ncyc=3):
        """Configure burst of <ncyc> pulses separated by <ns> ns, repeating 1/sec."""
        freq = 1e9 / ns
        pulse_width = 50e-9  # narrow pulse for dead-time testing

        self.output_off()
        self.cmd(":SOUR1:FUNC PULS")
        self.cmd(f":SOUR1:FREQ {freq}")
        self.cmd(":SOUR1:VOLT:HIGH 3.3")
        self.cmd(":SOUR1:VOLT:LOW 0")
        self.cmd(f":SOUR1:PULS:WIDT {pulse_width}")

        # N-cycle burst, internal trigger, 1 second between bursts
        self.cmd(":SOUR1:BURS:STAT ON")
        self.cmd(":SOUR1:BURS:MODE TRIG")
        self.cmd(f":SOUR1:BURS:NCYC {ncyc}")
        self.cmd(":SOUR1:BURS:INT:PER 1")
        self.cmd(":SOUR1:BURS:TRIG:SOUR INT")
        self.output_on()

        self.verify([
            ":SOUR1:FUNC?", ":SOUR1:FREQ?", ":SOUR1:VOLT:HIGH?",
            ":SOUR1:VOLT:LOW?", ":SOUR1:PULS:WIDT?",
            ":SOUR1:BURS:STAT?", ":SOUR1:BURS:MODE?",
            ":SOUR1:BURS:NCYC?", ":SOUR1:BURS:INT:PER?",
            ":SYST:ERR?",
        ])


def cmd_periodic(sg, args):
    """Configure channel 1: continuous pulse at <hz> Hz, 0-3.3V, 1ms width."""
    sg.periodic(args.hz)
    print(f"Output ON: {args.hz} Hz continuous")


def cmd_1pps(sg, args):
    """Configure channel 1: 1 Hz pulse, 0-3.3V, 1ms width."""
    sg.periodic(1)
    print("Output ON: 1 PPS continuous")


def cmd_burst(sg, args):
    """Burst of pulses separated by <ns> nanoseconds, once/sec."""
    freq = 1e9 / args.ns
    print(f"Burst: {args.ncyc} pulses, {args.ns} ns spacing (freq={freq:.1f} Hz)")

    sg.pulse_burst(args.ns, ncyc=args.ncyc)
    print("Output ON: pulse bursts, repeating 1/sec")


def cmd_off(sg, args):
    """Turn off channel 1 output."""
    sg.output_off()
    print("Output OFF")


def main():
    parser = argparse.ArgumentParser(description="Control a Rigol DG1022Z")
    parser.add_argument("--host", default="siggen")
    sub = parser.add_subparsers(dest="command", required=True)

    periodic = sub.add_parser("periodic", help="Continuous pulse at given Hz")
    periodic.add_argument("hz", type=float, help="Pulse frequency in Hz")
    sub.add_parser("1pps", help="1 Hz continuous pulse (shorthand for 'periodic 1')")
    sub.add_parser("off", help="Turn off output")

    burst = sub.add_parser("burst", help="Burst of pulses N ns apart")
    burst.add_argument("ns", type=int, help="Spacing between pulses in nanoseconds")
    burst.add_argument("ncyc", type=int, help="Number of pulses per burst")

    args = parser.parse_args()

    commands = {
        "periodic": cmd_periodic,
        "1pps": cmd_1pps,
        "burst": cmd_burst,
        "off": cmd_off,
    }

    with Siggen(host=args.host) as sg:
        print(f"Connected to: {sg.idn()}")
        commands[args.command](sg, args)


if __name__ == "__main__":
    main()
