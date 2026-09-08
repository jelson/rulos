#!/usr/bin/env python3
"""Host-only checks for serial metadata shared by the paired control utilities."""

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
