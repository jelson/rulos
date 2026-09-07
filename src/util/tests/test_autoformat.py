"""Regression tests for formatter file selection, without invoking formatters."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest

FORMATTER = Path(__file__).resolve().parents[1] / "autoformat"


class AutoformatTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="rulos-autoformat-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        for filename in ("src/owned.c", "ext/vendor.c", "ext/vendor.py"):
            path = self.root / filename
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture\n")
        self.log = self.root / "format.log"
        tools = self.root / "bin"
        tools.mkdir()
        for name in ("clang-format", "black", "dos2unix"):
            command = tools / name
            command.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$FORMAT_LOG"\n')
            command.chmod(0o755)
        self.env = {**os.environ, "PATH": str(tools) + ":" + os.environ["PATH"]}
        self.env["FORMAT_LOG"] = str(self.log)

    def run_formatter(self, *args):
        subprocess.run(
            [str(FORMATTER), *args],
            cwd=self.root,
            env=self.env,
            check=True,
            capture_output=True,
            text=True,
        )
        calls = self.log.read_text() if self.log.exists() else ""
        self.assertNotIn("ext/", calls)
        return calls

    def test_explicit_check_skips_vendor_files(self):
        calls = self.run_formatter("--check", "ext/vendor.c", "./ext/vendor.py", "src/owned.c")
        self.assertIn("src/owned.c", calls)

    def test_explicit_format_skips_vendor_files(self):
        calls = self.run_formatter("ext/vendor.c", "ext/vendor.py", "src/owned.c")
        self.assertIn("src/owned.c", calls)

    def test_staged_selection_skips_vendor_files(self):
        subprocess.run(["git", "add", "src", "ext"], cwd=self.root, check=True)
        self.assertIn("src/owned.c", self.run_formatter("--check"))

    def test_vendor_only_selection_calls_no_formatter(self):
        self.assertEqual(self.run_formatter("--check", "ext/vendor.c", "ext/vendor.py"), "")


if __name__ == "__main__":
    unittest.main()
