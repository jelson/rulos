#!/usr/bin/env python3
"""No-pulse LectroTIC-4 control-path hardware regressions.

Requires pyserial and the application already flashed for this board's clock
and pins. No pulse generator is needed. The optional --uart port must have TX
wired to the timestamper's serial input, with a common ground.

  control_test.py --port /dev/serial/by-id/usb-Lectrobox_...-if00
  control_test.py --port ... --uart /dev/serial/by-id/usb-Black_Magic...-if02
  control_test.py --port ... --persistence --elf build/.../timestamper.elf

The USB tests discard pending captures but restore live configuration. They
never save, reset, or flash firmware. --persistence explicitly permits config
sector writes and BMP resets: it saves a baseline, forces HAL erase/program
failures, retries identical saves, checks reset persistence, then saves the
original live channel settings again. Back up flash first if the distinction
between original saved and original live settings matters. No option bytes,
flash protection, mass erase, or firmware flashing are used.

Persistence requires Python-enabled gdb-multiarch, a BMP on this same H523,
and the exact running ELF with HAL_FLASHEx_Erase/HAL_FLASH_Program symbols.
Use a stable by-id --port because resets can change ttyACM numbering.
"""

import argparse
from pathlib import Path
import re
import shlex
import signal
import subprocess
import sys
import tempfile
import threading
import time

import serial
import serial.tools.list_ports

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "util"))
sys.path.insert(0, str(HERE.parents[2] / "util"))

import bmpflash
import tsctl

NO_ERROR = '0,"No error"'
COMMAND_ERROR = '-100,"Command error"'
SAVE_ERROR = '-240,"Configuration save failed"'
SERIAL_RECORD = re.compile(r"S \d+\.\d{9} (.*)")
PULSE_RECORD = re.compile(r"[0-3] \d+\.\d{9} [+-]")


def require(ok, detail):
    if not ok:
        raise RuntimeError(detail)


def lines_from_buffer(buf):
    """Remove complete ASCII lines, preserving a partial USB read for next time."""
    lines = []
    while b"\n" in buf:
        end = buf.index(b"\n")
        lines.append(bytes(buf[:end]).rstrip(b"\r").decode("ascii"))
        del buf[: end + 1]
    return lines


def detect_port():
    ports = [
        port.device
        for port in serial.tools.list_ports.comports()
        if port.vid == tsctl.TIMESTAMPER_VID and port.pid == tsctl.TIMESTAMPER_PID
    ]
    require(len(ports) == 1, f"expected one LectroTIC-4 USB port, found {ports}; use --port")
    return ports[0]


class Control:
    def __init__(self, port, timeout):
        self.port = port
        self.timeout = timeout
        self.ser = None
        self.open()

    def open(self):
        self.ser = serial.Serial(
            self.port, 115200, timeout=0.02, write_timeout=self.timeout, exclusive=True
        )

    def close(self):
        if self.ser is not None:
            self.ser.close()
            self.ser = None

    def write(self, payload):
        require(self.ser.write(payload) == len(payload), "short USB write")

    def drain(self, quiet=0.15, timeout=2.0):
        data = bytearray()
        deadline = time.monotonic() + timeout
        last = time.monotonic()
        while time.monotonic() < deadline:
            chunk = self.ser.read(4096)
            if chunk:
                data.extend(chunk)
                last = time.monotonic()
            elif time.monotonic() - last >= quiet:
                return bytes(data)
        raise RuntimeError("USB did not become quiet")

    def quiet(self):
        # Query before disabling so the caller can restore the real entry state.
        # Any old binary capture bytes precede the final ASCII query response.
        self.write(b"OUTP:STAT?\nOUTP:STAT OFF\n")
        data = self.drain(timeout=self.timeout)
        # Startup notification can take the stream's TX turn immediately after
        # the reply, before OFF dispatches. It is not a second query response.
        data = re.sub(rb"(?m)^# Starting LectroTIC-4, version [^\r\n]*\r?\n", b"", data)
        require(data.endswith((b"0\n", b"1\n")), f"no output-state reply: {data[-100:]!r}")
        require(
            re.search(rb"(?m)^[01]\r?$", data[:-2]) is None,
            f"extra output-state reply: {data[-100:]!r}",
        )
        return chr(data[-2])

    def exchange(self, commands, expected, delay=0, extra=None, extras_done=None):
        """Read concurrently with large writes, checking every reply and extra byte."""
        payload = ("\n".join(commands) + "\n").encode("ascii")
        errors = []

        def write():
            try:
                self.write(payload)
            except Exception as exc:
                errors.append(exc)

        writer = threading.Thread(target=write)
        writer.start()
        deadline = time.monotonic() + self.timeout
        buf = bytearray()
        got = []
        last = time.monotonic()
        try:
            time.sleep(delay)
            while time.monotonic() < deadline:
                chunk = self.ser.read(4096)
                if chunk:
                    last = time.monotonic()
                    buf.extend(chunk)
                    for line in lines_from_buffer(buf):
                        if extra and extra(line):
                            continue
                        index = len(got)
                        require(index < len(expected), f"unexpected extra reply: {line!r}")
                        require(
                            line == expected[index],
                            f"reply {index}: expected {expected[index]!r}, got {line!r}",
                        )
                        got.append(line)
                complete = len(got) == len(expected) and not writer.is_alive()
                if complete and (extras_done is None or extras_done()):
                    if time.monotonic() - last >= 0.15:
                        break
                if errors:
                    raise errors[0]
            require(not writer.is_alive(), "USB command write timed out under backpressure")
            require(not errors, f"USB command write failed: {errors}")
            require(len(got) == len(expected), f"received {len(got)} of {len(expected)} replies")
            require(not buf, f"trailing partial reply: {bytes(buf)!r}")
            require(extras_done is None or extras_done(), "missing UART records")
            return got
        finally:
            if writer.is_alive():
                self.ser.cancel_write()
            writer.join(self.timeout + 1)
            require(not writer.is_alive(), "USB writer did not stop after cancellation")

    def query(self, command):
        self.write((command + "\n").encode())
        data = self.drain(timeout=self.timeout)
        require(data.endswith(b"\n") and data.count(b"\n") == 1, f"{command}: {data!r}")
        return data.decode("ascii").rstrip("\r\n")

    def channels(self):
        return [(self.query(f"INP{c}:SLOP?"), self.query(f"INP{c}:DIV?")) for c in range(4)]

    def set_channels(self, channels):
        commands = []
        for c, (slope, divider) in enumerate(channels):
            commands += [f"INP{c}:SLOP {slope}", f"INP{c}:DIV {divider}"]
        self.exchange(commands + ["SYST:ERR?"], [NO_ERROR])

    def reset(self, bmp):
        self.close()
        require(bmpflash.reset(port=bmp) == 0, "BMP reset failed")
        time.sleep(3)
        deadline = time.monotonic() + 45
        last = None
        while time.monotonic() < deadline:
            try:
                self.open()
                self.quiet()
                require(
                    self.query("*IDN?").startswith(tsctl.IDN_PREFIX), "wrong device after reset"
                )
                return
            except (OSError, serial.SerialException, RuntimeError) as exc:
                last = exc
                self.close()
                time.sleep(0.5)
        raise RuntimeError(f"USB did not reconnect after reset: {last}")


def test_usb(control, count, delay):
    idn = control.query("*IDN?")
    require(idn.startswith(tsctl.IDN_PREFIX), f"not a LectroTIC-4: {idn!r}")
    commands = ["*IDN?", "SER:BAUD?", "FORM:DATA?", "OUTP:STAT?"]
    expected = [idn, control.query("SER:BAUD?"), "TEXT", "0"]
    require(len(("\n".join(commands) + "\n").encode()) <= 64, "burst exceeds one USB packet")
    control.exchange(commands, expected)
    print("PASS: ordered queries in one USB packet")
    control.exchange(commands * 32, expected * 32)
    print("PASS: ordered queries across many USB packets")
    control.exchange(
        ["*CLS", "FOOBAR", "*IDN?", "SYST:ERR?", "SYST:ERR?"],
        [idn, COMMAND_ERROR, NO_ERROR],
    )
    print("PASS: queued error reply is delivered and clears exactly once")
    control.exchange(commands * count, expected * count, delay=delay)
    print(f"PASS: {len(expected) * count} ordered replies after {delay:g}s without host reads")


def test_reconnect(control):
    control.write(b"SYST:ER")
    time.sleep(0.1)
    control.close()
    time.sleep(0.2)
    control.open()
    # The OS may retain an already-submitted response across DTR changes. It is
    # not a device-queued reply; flush those host-side bytes before the new query.
    control.ser.reset_input_buffer()
    idn = control.query("*IDN?")
    require(idn.startswith(tsctl.IDN_PREFIX), f"partial command survived reconnect: {idn!r}")
    control.exchange(["SYST:ERR?"], [NO_ERROR])
    print("PASS: reconnect discards the previous session's partial command")

    # Fill the host receive queue until endpoint completion stalls, leaving a
    # reply and the rest of its OUT packet owned by the old device session.
    cancelled = threading.Event()
    errors = []

    def send():
        try:
            control.write(b"*IDN?\n" * 65536)
        except Exception as exc:
            if not cancelled.is_set():
                errors.append(exc)

    writer = threading.Thread(target=send)
    writer.start()
    try:
        time.sleep(2)
    finally:
        cancelled.set()
        control.ser.cancel_write()
        writer.join(control.timeout + 1)
        require(not writer.is_alive(), "old-session USB writer did not stop")
    require(not errors, f"old-session USB writer failed: {errors}")
    control.ser.reset_output_buffer()
    control.close()
    time.sleep(0.2)
    control.open()
    control.ser.reset_input_buffer()
    late = control.drain()
    # DTR does not abort a submitted IN transfer; it can complete once. The OS
    # flush may leave only its trailing fragment, but no queued command may run.
    require(
        (idn + "\n").encode().endswith(late), f"old queued replies survived reconnect: {late!r}"
    )
    control.exchange(["FORM:DATA?", "SYST:ERR?"], ["TEXT", NO_ERROR])
    print("PASS: reconnect abandons backpressured replies and retained commands")


def test_uart(control, port):
    control.exchange(["SER:BAUD 115200", "SER:STAT ON", "OUTP:STAT ON", "SYST:ERR?"], [NO_ERROR])
    sent = [f"control-{i:04d}-" + "x" * 48 for i in range(32)]
    received = []
    errors = []
    stop = threading.Event()

    def classify(line):
        match = SERIAL_RECORD.fullmatch(line)
        if match:
            received.append(match[1])
            return True
        return PULSE_RECORD.fullmatch(line) is not None

    with serial.Serial(port, 115200, timeout=0.05, write_timeout=2) as uart:

        def send():
            try:
                for line in sent:
                    uart.write((line + "\n").encode())
                    if stop.wait(0.04):
                        return
            except Exception as exc:
                errors.append(exc)

        sender = threading.Thread(target=send)
        sender.start()
        try:
            control.exchange(
                ["SER:BAUD?", "FORM:DATA?"] * 512,
                ["115200", "TEXT"] * 512,
                extra=classify,
                extras_done=lambda: len(received) >= len(sent) and not sender.is_alive(),
            )
            require(not errors, f"UART writer failed: {errors}")
            require(received == sent, f"UART payload loss/reordering: {received!r}")
        finally:
            stop.set()
            sender.join(3)
            require(not sender.is_alive(), "UART writer did not stop")
            control.write(b"OUTP:STAT OFF\nSER:STAT OFF\n")
            control.drain()
    print("PASS: UART timestamp records and SCPI replies share USB without loss")


def fault_script(elf, bmp, symbol, ready):
    """Stop exactly at HAL entry before returning a synthetic HAL_ERROR."""
    return "\n".join([
        "set confirm off",
        "set pagination off",
        f"file {shlex.quote(str(elf))}",
        f"target extended-remote {bmp}",
        "monitor connect_rst disable",
        "monitor swd",
        "attach 1",
        f"hbreak *{symbol}",
        f"python open({str(ready)!r}, 'w').close()",
        "continue",
        "python",
        f"expected = int(gdb.parse_and_eval('(void *){symbol}')) & ~1",
        "if int(gdb.parse_and_eval('$pc')) != expected:",
        "    raise gdb.GdbError('Did not stop at the requested HAL entry')",
        "end",
        "return (HAL_StatusTypeDef)1",
        "delete breakpoints",
        "detach",
        "quit",
        "",
    ])


def inject_save_failure(control, elf, bmp, symbol):
    with tempfile.TemporaryDirectory(prefix="lt4-save-fault-") as tmp:
        tmp = Path(tmp)
        ready = tmp / "ready"
        script = tmp / "fault.gdb"
        script.write_text(fault_script(elf, bmp, symbol, ready))
        with (tmp / "gdb.log").open("w+") as log:
            proc = subprocess.Popen(
                ["gdb-multiarch", "--batch", "--nx", "-x", str(script)],
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                deadline = time.monotonic() + control.timeout
                while not ready.exists() and proc.poll() is None and time.monotonic() < deadline:
                    time.sleep(0.02)
                require(ready.exists(), "GDB did not arm the failure breakpoint")
                control.write(b"CONF:SAVE\n")
                require(proc.wait(timeout=control.timeout) == 0, "GDB failure injection failed")
                control.exchange(["SYST:ERR?", "SYST:ERR?"], [SAVE_ERROR, NO_ERROR])
            except BaseException:
                if proc.poll() is None:
                    proc.send_signal(signal.SIGINT)
                    try:
                        proc.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait()
                log.seek(0)
                print(log.read(), file=sys.stderr)
                # Recover a possibly halted target before the caller restores its state.
                control.reset(bmp)
                raise


def test_persistence(control, elf, bmp):
    idn = control.query("*IDN?").split(",")
    uid = bmpflash.read_serial(0x08FFF800, "LT4-", port=bmp)
    require(len(idn) == 4 and uid == idn[2], f"BMP UID {uid!r} does not match USB IDN {idn!r}")
    baseline = control.channels()
    control.exchange(["CONF:SAVE", "SYST:ERR?"], [NO_ERROR])
    try:
        for symbol in ("HAL_FLASHEx_Erase", "HAL_FLASH_Program"):
            wanted = control.channels()
            slope, divider = wanted[1]
            wanted[1] = (slope, str(int(divider) % 65535 + 1))
            control.set_channels(wanted)
            inject_save_failure(control, elf, bmp, symbol)
            require(control.channels() == wanted, "failed save changed the live configuration")
            # No reset or configuration change here: the stale saved-cache bug
            # would make this identical retry falsely succeed without writing.
            control.exchange(["CONF:SAVE", "SYST:ERR?"], [NO_ERROR])
            control.reset(bmp)
            require(control.channels() == wanted, "identical retry did not survive reset")
            print(f"PASS: {symbol} failure reported, identical retry survives reset")
    finally:
        control.set_channels(baseline)
        control.exchange(["CONF:SAVE", "SYST:ERR?"], [NO_ERROR])


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--port", help="LectroTIC-4 USB CDC port; default: autodetect")
    parser.add_argument("--uart", help="optional auxiliary UART source port")
    parser.add_argument(
        "--burst-count", type=int, default=2048, help="four-query blocks in the delayed-read test"
    )
    parser.add_argument("--read-delay", type=float, default=1, help="seconds to delay USB reads")
    parser.add_argument(
        "--timeout", type=float, default=45, help="seconds allowed per test or debug operation"
    )
    parser.add_argument(
        "--persistence", action="store_true", help="permit config flash writes and BMP resets"
    )
    parser.add_argument("--elf", type=Path, help="exact running ELF, required with --persistence")
    parser.add_argument("--bmp", help="BMP GDB port; default: autodetect")
    args = parser.parse_args()
    if args.burst_count < 1 or args.read_delay < 0 or args.timeout <= args.read_delay:
        parser.error("burst count and timeout must be positive, with 0 <= read delay < timeout")
    if args.persistence and (args.elf is None or not args.elf.is_file()):
        parser.error("--persistence requires --elf pointing to the exact running firmware")
    if not args.persistence and (args.elf or args.bmp):
        parser.error("--elf and --bmp require explicit --persistence permission")
    control = Control(args.port or detect_port(), args.timeout)
    snapshot = None
    try:
        output = control.quiet()
        snapshot = {
            "channels": control.channels(),
            "format": control.query("FORM:DATA?"),
            "serial": control.query("SER:STAT?"),
            "baud": control.query("SER:BAUD?"),
            "output": output,
        }
        control.exchange(["SER:STAT OFF", "FORM:DATA TEXT", "OUTP:CLE"], ["# output cleared"])
        control.exchange(["*CLS", "SYST:ERR?"], [NO_ERROR])
        test_usb(control, args.burst_count, args.read_delay)
        test_reconnect(control)
        if args.uart:
            test_uart(control, args.uart)
        else:
            print("SKIP: UART competition (no --uart)")
        if args.persistence:
            test_persistence(control, args.elf.resolve(), args.bmp or bmpflash.detect_bmp())
        else:
            print("SKIP: flash failure/retry persistence (requires --persistence --elf)")
    finally:
        try:
            if snapshot is not None and control.ser is not None:
                control.write(b"OUTP:STAT OFF\n")
                control.drain()
                control.set_channels(snapshot["channels"])
                control.exchange(
                    [
                        f"FORM:DATA {snapshot['format']}",
                        f"SER:BAUD {snapshot['baud']}",
                        f"SER:STAT {snapshot['serial']}",
                        "SYST:ERR?",
                    ],
                    [NO_ERROR],
                )
                control.write(f"OUTP:STAT {snapshot['output']}\n".encode())
                time.sleep(0.1)
        finally:
            control.close()


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, serial.SerialException, subprocess.TimeoutExpired) as exc:
        sys.exit(f"FAIL: {exc}")
