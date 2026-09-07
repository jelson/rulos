#!/usr/bin/env python3
"""Hardware-free regressions: python3 -m unittest discover -s src/util/tests -v.

Command-file integration tests use gdb-multiarch when installed, without
connecting to a probe or starting an inferior.
"""

from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import bmpflash


def capture_script(*, elf=None, load=False, mass_erase=False, returncode=0):
    captured = {}

    def run(cmd, **kwargs):
        captured["argv"] = cmd
        captured["env"] = kwargs["env"]
        captured["path"] = Path(cmd[cmd.index("-x") + 1])
        captured["script"] = captured["path"].read_text()
        return subprocess.CompletedProcess(cmd, returncode)

    with patch.object(bmpflash.subprocess, "run", side_effect=run):
        captured["status"] = bmpflash._gdb("/dev/test-probe", elf, load, mass_erase)
    return captured


class BmpFlashTests(unittest.TestCase):
    def test_flash_is_one_batch_script(self):
        result = capture_script(elf="firmware with spaces.elf", load=True)
        self.assertEqual(result["status"], 0)
        self.assertIn("--batch", result["argv"])
        self.assertIn("--nx", result["argv"])
        self.assertNotIn("-ex", result["argv"])
        self.assertEqual(result["env"]["LC_ALL"], "C")
        self.assertFalse(result["path"].exists())
        self.assertEqual(
            result["script"],
            "set confirm off\nset pagination off\n"
            "file 'firmware with spaces.elf'\n"
            "tar ext /dev/test-probe\nmon conn enable\nmon swd\nat 1\n"
            "load\n" + bmpflash._VERIFY_FLASH + "\nmon reset\nkill\nquit\n",
        )

    def test_failure_status_is_returned(self):
        self.assertEqual(capture_script(returncode=1)["status"], 1)

    def test_erase_is_explicit_and_after_file_and_attach(self):
        self.assertNotIn("erase_mass", capture_script(elf="fw.elf", load=True)["script"])
        script = capture_script(elf="fw.elf", load=True, mass_erase=True)["script"]
        self.assertLess(script.index("file "), script.index("at 1"))
        self.assertIn("at 1\nmon erase_mass\nload\n", script)

    def test_reset_does_not_load_or_verify(self):
        script = capture_script()["script"]
        self.assertNotIn("file ", script)
        self.assertNotIn("load", script)
        self.assertNotIn("compare-sections", script)
        self.assertNotIn("erase_mass", script)
        self.assertTrue(script.endswith("mon reset\nkill\nquit\n"))


@unittest.skipUnless(shutil.which("gdb-multiarch"), "gdb-multiarch is not installed")
class GdbIntegrationTests(unittest.TestCase):
    def run_script(self, script):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".gdb") as commands:
            commands.write(script)
            commands.flush()
            return subprocess.run(
                ["gdb-multiarch", "--batch", "--nx", "-x", commands.name],
                capture_output=True,
                text=True,
                timeout=10,
                env={**bmpflash.os.environ, "LC_ALL": "C"},
            )

    def assert_stopped(self, result):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("REACHED RESET", result.stdout)

    def test_missing_elf_stops_before_connect_or_erase(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "missing firmware.elf"
            script = capture_script(elf=missing, load=True, mass_erase=True)["script"]
            script = script.replace("tar ext /dev/test-probe", "echo REACHED CONNECT\\n")
            result = self.run_script(script)
        self.assert_stopped(result)
        self.assertNotIn("REACHED CONNECT", result.stdout)
        self.assertIn("No such file", result.stderr)

    def test_load_error_stops_before_reset_and_quit(self):
        # An ordinary host ELF has no writable remote target, so load fails.
        result = self.run_script(
            f"set confirm off\nfile {shlex.quote(sys.executable)}\n"
            "load\necho REACHED RESET\\n\nquit\n"
        )
        self.assert_stopped(result)

    def test_quoted_elf_path(self):
        with tempfile.TemporaryDirectory() as directory:
            elf = Path(directory) / "firmware's copy.elf"
            elf.symlink_to(sys.executable)
            script = capture_script(elf=elf)["script"].split("tar ext ", 1)[0]
            result = self.run_script(script + "echo FILE OPENED\\n\nquit\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("FILE OPENED", result.stdout)

    def test_verification_results(self):
        matched = "Section .text, range 0x8000000 -- 0x8001000: matched.\n"
        cases = {
            "matched": (matched, True),
            "multiple matched": (
                matched + "Section .data, range 0x8001000 -- 0x8001100: matched.\n",
                True,
            ),
            "mismatch": (matched.replace("matched.", "MIS-MATCHED!"), False),
            "mixed": (
                matched + "Section .data, range 0x8001000 -- 0x8001100: MIS-MATCHED!\n",
                False,
            ),
            "no sections": ("", False),
            "unrecognized output": ("Verification unavailable\n", False),
            "warning": (matched + "warning: incomplete comparison\n", False),
        }
        for name, (output, success) in cases.items():
            with self.subTest(name=name):
                # Stub only the hardware-dependent comparison. GDB executes
                # the actual verifier and controls command-file error handling.
                result = self.run_script(
                    f"python gdb.execute = lambda *args, **kwargs: {output!r}\n"
                    + bmpflash._VERIFY_FLASH
                    + "\necho REACHED RESET\\n\nquit\n"
                )
                self.assertIn(output.strip(), result.stdout)
                if success:
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn("REACHED RESET", result.stdout)
                else:
                    self.assert_stopped(result)
                    self.assertIn("Flash verification failed", result.stderr)

    def test_verification_read_error(self):
        result = self.run_script(
            'python\ndef fail(*args, **kwargs):\n    raise gdb.error("Cannot access memory")\n'
            "gdb.execute = fail\nend\n" + bmpflash._VERIFY_FLASH + "\necho REACHED RESET\\n\nquit\n"
        )
        self.assert_stopped(result)
        self.assertIn("Cannot access memory", result.stderr)


if __name__ == "__main__":
    unittest.main()
