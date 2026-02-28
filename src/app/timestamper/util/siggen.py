#!/usr/bin/env python3

"""Control a Rigol DG1022Z signal generator over LAN.

Usage:
  siggen.py 1pps               1 Hz continuous pulse, 3.3V, 1ms width
  siggen.py pair_ns <ns>        Burst of 2 pulses <ns> apart, repeating 1/sec
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
        """Turn off output, settling to 0V DC first to avoid a transient."""
        self.cmd(f":SOUR{ch}:FUNC DC")
        self.cmd(f":SOUR{ch}:VOLT:OFFS 0")
        self.cmd(f":OUTP{ch} OFF")

    def verify(self, queries):
        for q in queries:
            print(f"  {q:30s} {self.query(q)}")

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


def cmd_1pps(sg, args):
    """Configure channel 1: 1 Hz pulse, 0-3.3V, 1ms width."""
    sg.output_off()
    sg.cmd(":SOUR1:BURS:STAT OFF")
    sg.cmd(":SOUR1:FUNC PULS")
    sg.cmd(":SOUR1:FREQ 1")
    sg.cmd(":SOUR1:VOLT:HIGH 3.3")
    sg.cmd(":SOUR1:VOLT:LOW 0")
    sg.cmd(":SOUR1:PULS:WIDT 0.001")
    sg.output_on()

    sg.verify([
        ":SOUR1:FUNC?", ":SOUR1:FREQ?", ":SOUR1:VOLT:HIGH?",
        ":SOUR1:VOLT:LOW?", ":SOUR1:PULS:WIDT?", ":SOUR1:BURS:STAT?",
        ":SYST:ERR?",
    ])
    print("Output ON: 1 PPS continuous")


def cmd_pair_ns(sg, args):
    """Burst of pulses separated by <ns> nanoseconds, once/sec."""
    ns = args.ns
    ncyc = args.ncyc
    freq = 1e9 / ns
    print(f"Burst: {ncyc} pulses, {ns} ns spacing (freq={freq:.1f} Hz)")

    sg.pulse_burst(ns, ncyc=ncyc)
    print("Output ON: pulse bursts, repeating 1/sec")


def cmd_off(sg, args):
    """Turn off channel 1 output."""
    sg.output_off()
    print("Output OFF")


def main():
    parser = argparse.ArgumentParser(description="Control a Rigol DG1022Z")
    parser.add_argument("--host", default="siggen")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("1pps", help="1 Hz continuous pulse")
    sub.add_parser("off", help="Turn off output")

    pair = sub.add_parser("pair_ns", help="Burst of pulses N ns apart")
    pair.add_argument("ns", type=int, help="Spacing between pulses in nanoseconds")
    pair.add_argument("--ncyc", type=int, default=3, help="Number of pulses (default: 3)")

    args = parser.parse_args()

    commands = {
        "1pps": cmd_1pps,
        "pair_ns": cmd_pair_ns,
        "off": cmd_off,
    }

    with Siggen(host=args.host) as sg:
        print(f"Connected to: {sg.idn()}")
        commands[args.command](sg, args)


if __name__ == "__main__":
    main()
