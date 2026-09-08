#!/usr/bin/env python3
"""Host-only checks for the control hardware regression's transport and guards."""

import io
from pathlib import Path
import threading
import unittest
from unittest.mock import patch

import control_test as control


class FakePort:
    def __init__(self, replies, capacity=128):
        self.replies = replies
        self.capacity = capacity
        self.buffer = bytearray()
        self.condition = threading.Condition()
        self.cancelled = False

    def write(self, payload):
        reply = self.replies(payload)
        with self.condition:
            while reply and not self.cancelled:
                while len(self.buffer) == self.capacity and not self.cancelled:
                    self.condition.wait()
                room = self.capacity - len(self.buffer)
                self.buffer.extend(reply[:room])
                reply = reply[room:]
                self.condition.notify_all()
        return len(payload)

    def read(self, count):
        with self.condition:
            if not self.buffer:
                self.condition.wait(0.002)
            # Deliberately split complete replies across transport reads.
            count = min(count, 7)
            result = bytes(self.buffer[:count])
            del self.buffer[:count]
            self.condition.notify_all()
            return result

    def cancel_write(self):
        with self.condition:
            self.cancelled = True
            self.condition.notify_all()


def connection(replies, capacity=128):
    result = control.Control.__new__(control.Control)
    result.timeout = 0.4
    result.ser = FakePort(replies, capacity)
    return result


class TransportTests(unittest.TestCase):
    def test_partial_lines(self):
        data = bytearray(b"first\r\nsecond\npar")
        self.assertEqual(control.lines_from_buffer(data), ["first", "second"])
        self.assertEqual(data, b"par")
        data.extend(b"tial\n")
        self.assertEqual(control.lines_from_buffer(data), ["partial"])
        self.assertFalse(data)

    def test_delayed_reads_unblock_large_write(self):
        conn = connection(lambda payload: payload, capacity=16)
        expected = [f"reply-{i:03d}" for i in range(128)]
        self.assertEqual(conn.exchange(expected, expected, delay=0.02), expected)

    def test_reordered_replies_fail(self):
        conn = connection(lambda payload: b"second\nfirst\n")
        with self.assertRaisesRegex(RuntimeError, "reply 0"):
            conn.exchange(["one?", "two?"], ["first", "second"])

    def test_duplicate_replies_fail(self):
        conn = connection(lambda payload: b"one\none\n")
        with self.assertRaisesRegex(RuntimeError, "unexpected extra reply"):
            conn.exchange(["one?"], ["one"])

    def test_missing_reply_fails(self):
        conn = connection(lambda payload: b"one\n")
        with self.assertRaisesRegex(RuntimeError, "received 1 of 2 replies"):
            conn.exchange(["one?", "two?"], ["one", "two"])

    def test_partial_trailing_reply_fails(self):
        conn = connection(lambda payload: b"one\npart")
        with self.assertRaisesRegex(RuntimeError, "trailing partial reply"):
            conn.exchange(["one?"], ["one"])

    def test_failed_writer_is_joined(self):
        def fail(payload):
            raise OSError("write failed")

        conn = connection(fail)
        with self.assertRaisesRegex(OSError, "write failed"):
            conn.exchange(["one?"], ["one"])

    def test_uart_records_do_not_count_as_replies(self):
        conn = connection(lambda payload: b"S 1.000000000 marker\nTEXT\n")
        records = []

        def classify(line):
            if line.startswith("S "):
                records.append(line)
                return True
            return False

        self.assertEqual(
            conn.exchange(["FORM?"], ["TEXT"], extra=classify, extras_done=lambda: bool(records)),
            ["TEXT"],
        )

    def test_quiet_preserves_entry_output_state(self):
        conn = connection(lambda payload: b"old binary\x00\xff1\n")
        self.assertEqual(conn.quiet(), "1")

    def test_quiet_allows_startup_marker_after_reply(self):
        conn = connection(lambda payload: b"1\n# Starting LectroTIC-4, version 1.0.0-test\n")
        self.assertEqual(conn.quiet(), "1")

    def test_quiet_does_not_hide_extra_reply(self):
        conn = connection(lambda payload: b"1\n1\n# Starting LectroTIC-4, version test\n")
        with self.assertRaisesRegex(RuntimeError, "extra output-state reply"):
            conn.quiet()

    def test_fault_return_is_guarded_by_exact_pc(self):
        script = control.fault_script(
            Path("/tmp/app.elf"), "/dev/probe", "HAL_FLASHEx_Erase", Path("/tmp/ready")
        )
        self.assertLess(script.index("hbreak *HAL_FLASHEx_Erase"), script.index("'w').close()"))
        self.assertLess(
            script.index("if int(gdb.parse_and_eval('$pc'))"),
            script.index("return (HAL_StatusTypeDef)1"),
        )
        self.assertNotIn("erase_mass", script)
        self.assertNotIn("load\n", script)

    def test_missing_persistence_permission_fails_before_port_open(self):
        with patch("sys.argv", ["control_test.py", "--elf", "/tmp/app.elf"]):
            with patch.object(control, "Control") as constructor, patch(
                "sys.stderr", io.StringIO()
            ):
                with self.assertRaises(SystemExit) as caught:
                    control.main()
                self.assertEqual(caught.exception.code, 2)
                constructor.assert_not_called()


if __name__ == "__main__":
    unittest.main()
