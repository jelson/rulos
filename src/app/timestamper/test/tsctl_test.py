#!/usr/bin/env python3
"""Host-only checks for timestamper queries and paired-control serial metadata."""

import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "util"))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "pulsegen", "util"))

import pgctl
import tsctl


class FakeClock:
    def __init__(self):
        self.now = 0.0

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


class FakeSerial:
    """Ordered wire arrivals independent of the host's read/reset timing."""

    def __init__(self, clock, arrivals=(), response=b"POS\n"):
        self.clock = clock
        self.timeout = 0.37
        self.arrivals = list(arrivals)
        self.response = response
        self.commands = []
        self.closed = False

    def write(self, data):
        self.commands.append(data)
        if data.endswith(b"?\n"):
            start = max(self.clock.now, self.arrivals[-1][0] if self.arrivals else 0)
            # Split the response too, so readline must wait through a partial line.
            self.arrivals.extend(
                [(start + 0.005, self.response[:1]), (start + 0.02, self.response[1:])]
            )

    def flush(self):
        pass

    def reset_input_buffer(self):
        self.arrivals = [(when, data) for when, data in self.arrivals if when > self.clock.now]

    def read(self, size):
        deadline = self.clock.now + self.timeout
        out = bytearray()
        while self.arrivals and self.arrivals[0][0] <= deadline:
            when, data = self.arrivals.pop(0)
            self.clock.now = max(self.clock.now, when)
            count = min(size - len(out), len(data))
            out += data[:count]
            if count < len(data):
                self.arrivals.insert(0, (when, data[count:]))
            if len(out) == size:
                return bytes(out)
        self.clock.now = deadline
        return bytes(out)

    def readline(self):
        timeout = self.timeout
        deadline = self.clock.now + timeout
        line = bytearray()
        try:
            while self.clock.now < deadline:
                self.timeout = deadline - self.clock.now
                byte = self.read(1)
                if not byte:
                    break
                line += byte
                if byte == b"\n":
                    break
        finally:
            self.timeout = timeout
        return bytes(line)

    def close(self):
        self.closed = True


class QueryTests(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock()
        self.enterContext(patch.object(tsctl.time, "monotonic", self.clock.monotonic))
        self.enterContext(patch.object(tsctl.time, "sleep", self.clock.sleep))

    def connection(self, arrivals=(), response=b"POS\n", stream_on=True):
        tic = tsctl.LectroTIC4.__new__(tsctl.LectroTIC4)
        tic._ser = FakeSerial(self.clock, arrivals, response)
        tic._stream_on = stream_on
        return tic

    def assert_stream_restored(self, tic, enabled=True):
        self.assertEqual(tic._stream_on, enabled)
        self.assertEqual(tic._ser.timeout, 0.37)
        self.assertNotIn(b"OUTP:CLE\n", tic._ser.commands)
        if enabled:
            self.assertEqual(tic._ser.commands[0], b"OUTP:STAT OFF\n")
            self.assertEqual(tic._ser.commands[-1], b"OUTP:STAT ON\n")
        else:
            self.assertFalse(any(cmd.startswith(b"OUTP:STAT") for cmd in tic._ser.commands))

    def test_delayed_text_fragments_are_not_query_replies(self):
        tic = self.connection(
            [(0, b"0 4.000"), (0.04, b"000000 +\n"), (0.08, b"1 5.000000000 -\n")]
        )
        self.assertEqual(tic.query("INP0:SLOP?"), "POS")
        self.assert_stream_restored(tic)
        self.assertFalse(tic._ser.arrivals)

    def test_delayed_binary_fragments_are_not_query_replies(self):
        tic = self.connection([(0, b"\x00\x00"), (0.04, b"\x00\x20\xff"), (0.08, b"\x00\n\x00")])
        self.assertEqual(tic.query("INP0:SLOP?"), "POS")
        self.assert_stream_restored(tic)
        self.assertFalse(tic._ser.arrivals)

    def test_already_disabled_stream_still_drains_old_data(self):
        tic = self.connection([(0.04, b"0 1.000000000 +\n")], stream_on=False)
        self.assertEqual(tic.query("INP0:SLOP?"), "POS")
        self.assert_stream_restored(tic, enabled=False)

    def test_host_scheduling_pause_does_not_count_as_observed_silence(self):
        tic = self.connection([(0, b"early\n"), (0.20, b"late\n")])
        samples = 0

        def monotonic():
            nonlocal samples
            samples += 1
            if samples == 4:
                # Pause after the first read's quiet deadline was recorded, before the next
                # iteration. New bytes arrive while this process is not scheduled.
                self.clock.sleep(0.2)
            return self.clock.monotonic()

        with patch.object(tsctl.time, "monotonic", monotonic):
            self.assertEqual(tic.query("INP0:SLOP?"), "POS")
        self.assert_stream_restored(tic)

    def test_drain_timeout_does_not_send_query(self):
        tic = self.connection([(i * 0.02, b"stale\n") for i in range(70)])
        with self.assertRaisesRegex(TimeoutError, "quiet"):
            tic.query("INP0:SLOP?")
        self.assert_stream_restored(tic)
        self.assertNotIn(b"INP0:SLOP?\n", tic._ser.commands)
        self.assertLessEqual(self.clock.now, 1.01)

    def test_drain_error_restores_stream_and_timeout(self):
        tic = self.connection()
        with patch.object(tic._ser, "read", side_effect=OSError("read failed")):
            with self.assertRaisesRegex(OSError, "read failed"):
                tic.query("INP0:SLOP?")
        self.assert_stream_restored(tic)

    def test_response_error_restores_stream_and_timeout(self):
        tic = self.connection()
        with patch.object(tic._ser, "readline", side_effect=OSError("reply failed")):
            with self.assertRaisesRegex(OSError, "reply failed"):
                tic.query("INP0:SLOP?")
        self.assert_stream_restored(tic)

    def test_query_write_error_restores_stream_and_timeout(self):
        tic = self.connection()
        write = tic._ser.write

        def fail_query(data):
            if data.endswith(b"?\n"):
                raise OSError("write failed")
            return write(data)

        with patch.object(tic._ser, "write", side_effect=fail_query):
            with self.assertRaisesRegex(OSError, "write failed"):
                tic.query("INP0:SLOP?")
        self.assert_stream_restored(tic)

    def test_disabled_stream_is_not_enabled_after_error(self):
        tic = self.connection(stream_on=False)
        with patch.object(tic._ser, "readline", side_effect=OSError("reply failed")):
            with self.assertRaisesRegex(OSError, "reply failed"):
                tic.query("INP0:SLOP?")
        self.assert_stream_restored(tic, enabled=False)

    def test_missing_or_partial_response_is_an_error(self):
        for response in (b"", b"PO"):
            with self.subTest(response=response):
                tic = self.connection(response=response)
                with self.assertRaisesRegex(TimeoutError, "response"):
                    tic.query("INP0:SLOP?")
                self.assert_stream_restored(tic)

    def test_autodetect_uses_same_drain_for_delayed_binary_data(self):
        ser = FakeSerial(
            self.clock,
            [(0.04, b"\x00\xff"), (0.08, b"\x00\n")],
            response=b"Lectrobox,LectroTIC-4,serial,test\n",
        )
        candidate = SimpleNamespace(
            device="/dev/test", vid=tsctl.TIMESTAMPER_VID, pid=tsctl.TIMESTAMPER_PID
        )
        with patch("serial.tools.list_ports.comports", return_value=[candidate]), patch(
            "serial.Serial", return_value=ser
        ):
            self.assertEqual(tsctl.autodetect_port(), "/dev/test")
        self.assertTrue(ser.closed)
        self.assertEqual(ser.timeout, 0.37)

    def test_autodetect_skips_device_that_never_becomes_quiet(self):
        noisy = FakeSerial(self.clock, [(i * 0.02, b"stale\n") for i in range(70)])
        quiet = FakeSerial(self.clock, response=b"Lectrobox,LectroTIC-4,serial,test\n")
        candidates = [
            SimpleNamespace(device=port, vid=tsctl.TIMESTAMPER_VID, pid=tsctl.TIMESTAMPER_PID)
            for port in ("/dev/noisy", "/dev/quiet")
        ]
        with patch("serial.tools.list_ports.comports", return_value=candidates), patch(
            "serial.Serial", side_effect=[noisy, quiet]
        ), patch("sys.stderr"):
            self.assertEqual(tsctl.autodetect_port(), "/dev/quiet")
        self.assertNotIn(b"*IDN?\n", noisy.commands)
        for ser in (noisy, quiet):
            self.assertTrue(ser.closed)
            self.assertEqual(ser.timeout, 0.37)


class UsbSerialTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.device = self.root / "ttyACM1"
        self.device.touch()
        self.alias = self.root / "serial" / "by-id" / "usb-Lectrobox"
        self.alias.parent.mkdir(parents=True)
        self.alias.symlink_to("../../ttyACM1")

    def assert_usb_serial(self, port, entries, expected):
        for client in (tsctl.LectroTIC4, pgctl.Pulsegen):
            with self.subTest(client=client.__name__):
                connection = client.__new__(client)
                connection._ser = SimpleNamespace(port=str(port))
                with patch("serial.tools.list_ports.comports", return_value=entries):
                    self.assertEqual(connection.usb_serial, expected)
                self.assertEqual(connection.port, str(port))

    def metadata(self, device, serial_number="STM32-unique-id"):
        return SimpleNamespace(device=str(device), serial_number=serial_number)

    def test_direct_device_path(self):
        self.assert_usb_serial(self.device, [self.metadata(self.device)], "STM32-unique-id")

    def test_stable_alias_selects_correct_metadata(self):
        entries = [
            self.metadata(self.root / "ttyACM0", "different-device"),
            self.metadata(self.device),
        ]
        self.assert_usb_serial(self.alias, entries, "STM32-unique-id")

    def test_enumerated_alias_matches_direct_path(self):
        self.assert_usb_serial(self.device, [self.metadata(self.alias)], "STM32-unique-id")

    def test_different_aliases_match_same_device(self):
        other_alias = self.root / "serial" / "by-path" / "usb-physical-port"
        other_alias.parent.mkdir()
        other_alias.symlink_to(self.alias)
        self.assert_usb_serial(other_alias, [self.metadata(self.alias)], "STM32-unique-id")

    def test_unknown_device_has_no_serial(self):
        self.assert_usb_serial(self.root / "ttyACM10", [self.metadata(self.device)], None)

    def test_empty_enumeration_has_no_serial(self):
        self.assert_usb_serial(self.alias, [], None)

    def test_unmatched_broken_alias_has_no_serial(self):
        broken_alias = self.root / "serial" / "by-id" / "usb-disconnected"
        broken_alias.symlink_to("../../ttyACM2")
        self.assert_usb_serial(broken_alias, [self.metadata(self.device)], None)

    def test_missing_serial_metadata_stays_none(self):
        self.assert_usb_serial(self.alias, [self.metadata(self.device, None)], None)


if __name__ == "__main__":
    unittest.main()
