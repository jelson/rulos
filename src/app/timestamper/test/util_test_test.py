#!/usr/bin/env python3
"""Hardware-free regression checks for the utility bench scorers."""

from contextlib import redirect_stdout
import io
import math
import unittest
from unittest.mock import Mock, patch

import util_test as bench


def phase_rows(times_ns):
    """Use the unchanged production Allan math, including its mean-period detrending."""
    origin = times_ns[0]
    mean_tick = (times_ns[-1] - origin) / (len(times_ns) - 1)
    errors = [stamp - origin - i * mean_tick for i, stamp in enumerate(times_ns)]
    return bench.allan.adev_table(errors, mean_tick)


def printed_rows(rows):
    return [
        f"{bench.tsctl.format_time_ns(tau * bench.tsctl.NS):>15}   {adev:10.3e}   {terms:8d}"
        for tau, adev, terms in rows
    ]


class AllanScoringTests(unittest.TestCase):
    def test_zero_and_bounded_quantization_across_sample_rates(self):
        for rate, divider in ((20_000, 1), (100_000, 4), (1_000_000, 32), (1_000, 1)):
            tau_ns = divider * bench.tsctl.NS / rate
            for quantized in (False, True):
                with self.subTest(rate=rate, divider=divider, quantized=quantized):
                    times = [
                        i * tau_ns + (bench.tstest.TICK_NS if quantized and i % 2 else 0)
                        for i in range(2048)
                    ]
                    rows = bench.parse_allan_rows(printed_rows(phase_rows(times)))
                    self.assertGreaterEqual(len(rows), 6)
                    self.assertTrue(bench.allan_rows_sane(rows))
                    if quantized and tau_ns < 100_000:
                        self.assertGreater(rows[0][1], 1e-5)

    def test_missing_duplicate_and_displaced_timestamps_fail(self):
        for tau_ns in (32_000, 40_000, 1_000_000):
            times = [i * tau_ns for i in range(2048)]
            middle = len(times) // 2
            displaced = list(times)
            displaced[middle] += tau_ns / 4
            defects = {
                "missing": times[:middle] + times[middle + 1 :],
                "duplicate": times[:middle] + [times[middle]] + times[middle:],
                "displaced": displaced,
            }
            for name, samples in defects.items():
                with self.subTest(tau_ns=tau_ns, defect=name):
                    rows = bench.parse_allan_rows(printed_rows(phase_rows(samples)))
                    self.assertFalse(bench.allan_rows_sane(rows))

    def test_tau_units_and_display_rounding(self):
        rows = bench.parse_allan_rows([
            "400 ns 0.000e+00 20",
            "40.000 us 1.414e-04 18",
            "0.100000 ms 5.657e-05 16",
            "1.000000000 s 1.000e-05 8",
        ])
        self.assertEqual([terms for _, _, terms in rows], [20, 18, 16, 8])
        for row, tau in zip(rows, (4e-7, 4e-5, 1e-4, 1.0)):
            self.assertAlmostEqual(row[0], tau)
        self.assertTrue(bench.allan_rows_sane(rows))

    def test_excess_adev_fails_at_short_and_long_taus(self):
        for tau in (32e-6, 40e-6, 100e-6, 1e-3, 1.0):
            bound = max(1e-5, math.sqrt(2) * bench.tstest.TICK_NS / bench.tsctl.NS / tau)
            with self.subTest(tau=tau):
                self.assertFalse(bench.allan_rows_sane([(tau, bound * 1.01, 100)]))

    def test_invalid_or_empty_rows_fail(self):
        for row in (
            (0, 0, 10),
            (-1, 0, 10),
            (math.nan, 0, 10),
            (math.inf, 0, 10),
            (1, math.nan, 10),
            (1, math.inf, 10),
            (1, -1e-6, 10),
            (1, 0, 7),
        ):
            with self.subTest(row=row):
                self.assertFalse(bench.allan_rows_sane([row]))
        self.assertFalse(bench.allan_rows_sane([]))

    def score_transcript(self, lines, rc=0):
        with (
            patch.object(bench, "FAILURES", []),
            patch.object(bench, "run_util", return_value=(rc, lines)),
            redirect_stdout(io.StringIO()),
        ):
            bench.test_allan_hardware(Mock())
            return not bench.FAILURES

    def test_hardware_wrapper_retains_completeness_frequency_exit_and_loss_checks(self):
        times = [i * 40_000 + (4 if i % 2 else 0) for i in range(2048)]
        rows = printed_rows(phase_rows(times))
        frequency = "# 2048 samples, mean frequency 100.0000000 kHz"
        healthy = [frequency] + rows
        self.assertTrue(self.score_transcript(healthy))
        self.assertFalse(self.score_transcript(healthy, rc=1))
        self.assertFalse(self.score_transcript([frequency] + rows[:5]))
        self.assertFalse(self.score_transcript(rows))
        self.assertFalse(self.score_transcript(healthy + ["# device reported loss mid-capture"]))
        self.assertFalse(self.score_transcript(healthy + ["OVERRUN"]))
        self.assertFalse(self.score_transcript(healthy + ["1.000000000 s 1.100e-05 100"]))


if __name__ == "__main__":
    unittest.main()
