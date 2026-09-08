#!/usr/bin/env python3
"""Host-only checks for paired pulsegen regressions; no serial devices are opened."""

import io
import unittest
from unittest.mock import patch

import regression_test as regression


def timestamps(records):
    return [
        regression.Timestamp(ch, timestamp // regression.NS, timestamp % regression.NS, polarity)
        for ch, timestamp, polarity in records
    ]


def pulses(starts, width, ch=0):
    return [
        (ch, timestamp + offset, polarity)
        for timestamp in starts
        for offset, polarity in ((0, "+"), (width, "-"))
    ]


def bursts(starts, ncyc=regression.REP_START_NCYC):
    return [
        (0, start + n * regression.REP_START_SPACING_NS, "+")
        for start in starts
        for n in range(ncyc)
    ]


class Records:
    def __init__(self, events, records):
        self.events = events
        self.records = iter(records)

    def __iter__(self):
        return self

    def __next__(self):
        result = next(self.records)
        self.events.append("read")
        return result

    def close(self):
        self.events.append("close")


class FakeTimestamper:
    def __init__(self, events, records):
        self.events = events
        self.records = records
        self.windows = []

    def read_for(self, duration_s):
        self.windows.append(duration_s)
        self.events.append(
            "arm"
        )  # Models eager _begin_stream / OUTP:CLE, not iterator consumption.
        records = self.records() if callable(self.records) else self.records
        return Records(self.events, records)

    def set_slope(self, ch, slope):
        self.events.append(("slope", ch, slope))

    def set_divider(self, ch, divider):
        self.events.append(("divider", ch, divider))

    def discard_pending(self, settle_s):
        self.events.append(("discard", settle_s))


class FakePulsegen:
    def __init__(self, events):
        self.events = events
        self.period_ns = 0
        self.width_ns = 0
        self.rep_s = 0

    def off(self):
        self.events.append("off")

    def set_mode(self, mode):
        self.events.append(("mode", mode))

    def set_burst_state(self, ch, enabled):
        self.events.append(("burst", ch, enabled))

    def set_period(self, ch, seconds):
        self.period_ns = round(seconds * regression.NS)
        self.events.append(("period", ch, self.period_ns))

    def set_width(self, ch, seconds):
        self.width_ns = round(seconds * regression.NS)
        self.events.append(("width", ch, self.width_ns))

    def set_delay(self, ch, seconds):
        self.events.append(("delay", ch, seconds))

    def set_state(self, ch, enabled):
        self.events.append(("state", ch, enabled))

    def set_burst_ncycles(self, ch, ncyc):
        self.events.append(("cycles", ch, ncyc))

    def set_burst_period(self, ch, rep_s):
        self.rep_s = rep_s
        self.events.append(("repeat", ch, rep_s))

    def get_error(self):
        return '0,"No error"'


class CaptureTests(unittest.TestCase):
    def test_real_read_for_arms_before_start_without_opening_a_port(self):
        events = []
        expected = [(0, 4, "+"), (0, 12, "-")]
        ts = regression.LectroTIC4.__new__(regression.LectroTIC4)
        with patch.object(
            ts, "_begin_stream", side_effect=lambda: events.append("arm")
        ), patch.object(ts, "_stream_records", return_value=Records(events, timestamps(expected))):
            result = regression.collect_started(ts, 1, lambda: events.append("start"))
        self.assertEqual(result, expected)
        self.assertEqual(events, ["arm", "start", "read", "read", "close"])

    def test_start_follows_eager_arming_and_precedes_consumption(self):
        events = []
        expected = [(0, 4, "+"), (0, 12, "-")]
        ts = FakeTimestamper(events, timestamps(expected))
        result = regression.collect_started(ts, 1, lambda: events.append("start"))
        self.assertEqual(result, expected)
        self.assertEqual(events, ["arm", "start", "read", "read", "close"])

    def test_start_failure_closes_armed_iterator(self):
        events = []
        ts = FakeTimestamper(events, [])
        with self.assertRaisesRegex(RuntimeError, "start failed"):
            regression.collect_started(
                ts, 1, lambda: (_ for _ in ()).throw(RuntimeError("start failed"))
            )
        self.assertEqual(events, ["arm", "close"])

    def test_normal_collection_uses_same_decoder(self):
        events = []
        records = [(0, 4, "+"), (1, 12, "-")]
        ts = FakeTimestamper(events, timestamps(records))
        self.assertEqual(regression.collect(ts, 1, channels=(1,)), [records[1]])
        self.assertEqual(events, ["arm", "read", "read", "close"])

    def test_loss_on_unselected_channel_still_fails(self):
        ts = FakeTimestamper([], [regression.PulsesLost(3, 1, 0)])
        with self.assertRaisesRegex(RuntimeError, "lost pulses on ch3"):
            regression.collect(ts, 1, channels=(0,))

    def test_clock_failure_still_fails(self):
        ts = FakeTimestamper([], [regression.OscillatorFailure()])
        with self.assertRaisesRegex(RuntimeError, "oscillator failure"):
            regression.collect(ts, 1)

    def test_startup_phase_stages_off_then_arms_before_each_single_enable(self):
        events = []
        pg = FakePulsegen(events)
        ts = FakeTimestamper(
            events, lambda: timestamps(pulses([i * pg.period_ns for i in range(4)], pg.width_ns))
        )
        with patch.object(regression, "NUM_CHANNELS", 1), patch.object(
            regression, "prime_gp_prescalers", side_effect=lambda *args: events.append("prime")
        ), patch("sys.stdout", new=io.StringIO()):
            self.assertTrue(regression.test_gp_startup(ts, pg, 0.1))
        self.assertEqual(events.count("arm"), 2)
        for index, event in enumerate(events):
            if event == "arm":
                self.assertEqual(events[index + 1], ("state", 0, True))
                self.assertEqual(events[index + 2], "read")
                self.assertIn("off", events[:index])
        self.assertEqual(events.count(("state", 0, True)), 2)

    def test_priming_waits_for_observed_natural_periods(self):
        events = []
        records = [(0, t, "+") for t in (0, 50, 150, 250)]
        ts = FakeTimestamper(events, timestamps(records))
        pg = FakePulsegen(events)
        regression.prime_gp_prescalers(ts, pg, (0,), 100)
        self.assertEqual(events.count("read"), 4)
        self.assertEqual(events[-1], "close")

    def test_enable_phase_keeps_primed_period_and_uses_wide_pulses(self):
        events = []
        pg = FakePulsegen(events)
        # This fake emits records for every channel; collect_started must select only its DUT.
        ts = FakeTimestamper(
            events,
            lambda: timestamps([
                record
                for ch in range(regression.NUM_CHANNELS)
                for record in pulses([i * pg.period_ns for i in range(4)], pg.width_ns, ch)
            ]),
        )
        with patch.object(regression, "prime_gp_prescalers") as prime, patch(
            "sys.stdout", new=io.StringIO()
        ):
            self.assertTrue(regression.test_gp_enable(ts, pg, 0.1))
        self.assertEqual(
            [call.args[2:] for call in prime.call_args_list],
            [((ch,), 1_000_000_000) for ch in range(regression.NUM_CHANNELS)],
        )
        self.assertEqual(ts.windows, [3.3] * regression.NUM_CHANNELS)
        for ch in range(regression.NUM_CHANNELS):
            self.assertIn(("period", ch, 1_000_000_000), events)
            self.assertIn(("width", ch, 750_000_000), events)
        enabled = []
        for index, event in enumerate(events):
            if event == "arm":
                self.assertEqual(events[index + 1][0], "state")
                self.assertTrue(events[index + 1][2])
                self.assertEqual(events[index + 2], "read")
                enabled.append(events[index + 1][1])
        self.assertEqual(enabled, list(range(regression.NUM_CHANNELS)))
        self.assertIs(regression.TESTS["gp-enable"], regression.test_gp_enable)

    def test_enable_phase_rejects_an_extra_initial_pulse(self):
        events = []
        pg = FakePulsegen(events)
        records = pulses([0], 3852) + pulses(
            [4648 + i * regression.NS for i in range(4)], 750_000_000
        )
        ts = FakeTimestamper(events, timestamps(records))
        with patch.object(regression, "NUM_CHANNELS", 1), patch.object(
            regression, "prime_gp_prescalers"
        ), patch("sys.stdout", new=io.StringIO()):
            self.assertFalse(regression.test_gp_enable(ts, pg, 0.1))

    def test_repetition_phase_arms_before_both_single_enables(self):
        events = []
        pg = FakePulsegen(events)
        ts = FakeTimestamper(
            events, lambda: timestamps(bursts([0, round(pg.rep_s * regression.NS)]))
        )
        with patch.object(
            regression, "measure_burst", return_value=([3, 3], 0, [0.1])
        ) as prime, patch("sys.stdout", new=io.StringIO()):
            self.assertTrue(regression.test_rep_startup(ts, pg, 0.1))
        prime.assert_called_once()
        self.assertEqual(ts.windows, [41.0, 1.0])
        self.assertEqual(events.count("arm"), 2)
        for index, event in enumerate(events):
            if event == "arm":
                self.assertEqual(events[index + 1], ("state", 0, True))
                self.assertEqual(events[index + 2], "read")
        self.assertEqual(events.count(("state", 0, True)), 2)


class BurstBoundaryTests(unittest.TestCase):
    def score(self, records, delays=None, width=10_000, ncyc=3, sync=False, offsets=None):
        if delays is None:
            delays = {0: 0}
        with patch("sys.stdout", new=io.StringIO()):
            return regression.check_gp_burst_frame(
                records, delays, width, ncyc, 1_000_000, sync=sync, phase_offsets_ns=offsets
            )

    def test_complete_single_and_three_pulse_frames_have_no_boundary_trimming(self):
        for count in (1, 3):
            for delay in (0, 990_000):
                records = pulses([delay + i * 1_000_000 for i in range(count)], 10_000)
                self.assertTrue(self.score(records, {0: delay}, ncyc=count))
                self.assertFalse(self.score(records[:-1], {0: delay}, ncyc=count))

    def test_every_first_last_and_interior_fault_fails(self):
        records = pulses([0, 1_000_000, 2_000_000], 10_000)
        faulty = {
            "missing first pulse": records[2:],
            "missing last pulse": records[:-2],
            "missing first rise": records[1:],
            "missing last fall": records[:-1],
            "extra first pulse": pulses([-100_000], 3852) + records,
            "extra last pulse": records + pulses([3_000_000], 1000),
            "duplicate edge": records + [records[2]],
            "short first width": [(0, 0, "+"), (0, 3852, "-")] + records[2:],
            "short last width": records[:-1] + [(0, 2_003_852, "-")],
            "wrong interior period": pulses([0, 1_000_100, 2_000_000], 10_000),
            "repeated interior polarity": records[:3] + [(0, 1_010_000, "+")] + records[4:],
        }
        for name, edges in faulty.items():
            with self.subTest(name=name):
                self.assertFalse(self.score(edges))

    def test_sync_wide_frames_score_every_indexed_phase_without_pair_trimming(self):
        delays = dict(enumerate((0, 100_000, 200_000, 250_000)))
        records = [
            record
            for ch, delay in delays.items()
            for record in pulses([delay + i * 1_000_000 for i in range(3)], 750_000, ch)
        ]
        # Delivery order can be batched independently, but all finite-frame edges must survive.
        records = records[::2] + records[1::2]
        self.assertTrue(self.score(records, delays, 750_000, sync=True))
        shifted = [(ch, t + (1_000_000 if ch == 3 else 0), pol) for ch, t, pol in records]
        self.assertFalse(self.score(shifted, delays, 750_000, sync=True))
        self.assertFalse(self.score([r for r in records if r[0] != 0], delays, 750_000, sync=True))

    def test_fixed_offsets_require_independent_calibration_and_residual_stays_strict(self):
        delays = dict(enumerate((0, 100_000, 200_000, 250_000)))
        offsets = {0: 0, 1: -4, 2: 0, 3: -16}
        records = [
            record
            for ch, delay in delays.items()
            for record in pulses([delay + offsets[ch]], 750_000, ch)
        ]
        self.assertFalse(self.score(records, delays, 750_000, ncyc=1, sync=True))
        self.assertTrue(self.score(records, delays, 750_000, ncyc=1, sync=True, offsets=offsets))
        changed = [(ch, t + (16 if ch == 3 else 0), pol) for ch, t, pol in records]
        self.assertFalse(self.score(changed, delays, 750_000, ncyc=1, sync=True, offsets=offsets))

    def test_calibration_is_a_separate_flat_continuous_capture(self):
        events = []
        offsets = {0: 0, 1: -4, 2: 0, 3: -16}
        records = [
            record
            for ch, offset in offsets.items()
            for record in pulses([i * 1_000_000 + offset for i in range(1, 31)], 750_000, ch)
        ]
        ts, pg = FakeTimestamper(events, timestamps(records)), FakePulsegen(events)
        with patch("sys.stdout", new=io.StringIO()):
            measured = regression.measure_gp_sync_offsets(ts, pg, 750_000, 0.1)
        self.assertEqual(measured, offsets)
        self.assertEqual(ts.windows, [0.3])
        arm = events.index("arm")
        self.assertEqual(events[arm - 1], ("discard", 0.1))
        self.assertEqual(events[arm + 1], "read")
        for ch in offsets:
            self.assertIn(("state", ch, True), events[:arm])
            self.assertIn(("delay", ch, 0), events[:arm])
            self.assertIn(("width", ch, 750_000), events[:arm])
            self.assertNotIn(("burst", ch, True), events)

    def test_calibration_does_not_accept_a_broken_control_waveform(self):
        events = []
        records = [
            record
            for ch in range(4)
            for record in pulses(
                [i * 1_000_000 for i in range(30) if ch != 3 or i != 15], 750_000, ch
            )
        ]
        ts, pg = FakeTimestamper(events, timestamps(records)), FakePulsegen(events)
        with patch("sys.stdout", new=io.StringIO()), self.assertRaisesRegex(
            RuntimeError, "invalid GP SYNC control"
        ):
            regression.measure_gp_sync_offsets(ts, pg, 750_000, 0.1)
        self.assertEqual(events[-1], "off")

    def test_async_arm_precedes_its_only_enable_and_includes_flush_budget(self):
        events = []
        records = pulses([990_000], 10_000, 2)
        ts = FakeTimestamper(events, timestamps(records))
        pg = FakePulsegen(events)
        with patch("sys.stdout", new=io.StringIO()):
            self.assertTrue(
                regression.run_gp_burst_boundary_case(
                    ts, pg, regression.Pulsegen.ASYNC, {2: 990_000}, 10_000, 1, 0.01
                )
            )
        arm = events.index("arm")
        self.assertEqual(events[arm + 1 : arm + 3], [("state", 2, True), "read"])
        self.assertEqual(events.count(("state", 2, True)), 1)
        self.assertIn(("repeat", 2, 60.0), events[:arm])
        self.assertEqual(ts.windows, [0.3])
        self.assertEqual(events[-2:], ["close", "off"])

    def test_sync_configuration_frames_settle_before_one_armed_restart(self):
        events = []
        delays = dict(enumerate((0, 100_000, 200_000, 250_000)))
        records = [
            record
            for ch, delay in delays.items()
            for record in pulses([delay + i * 1_000_000 for i in range(3)], 750_000, ch)
        ]
        ts = FakeTimestamper(events, timestamps(records))
        pg = FakePulsegen(events)
        with patch.object(
            regression.time, "sleep", side_effect=lambda seconds: events.append(("settle", seconds))
        ), patch("sys.stdout", new=io.StringIO()):
            self.assertTrue(
                regression.run_gp_burst_boundary_case(
                    ts, pg, regression.Pulsegen.SYNC, delays, 750_000, 3, 60.0
                )
            )
        arm = events.index("arm")
        settle = events.index(("settle", 0.3))
        self.assertLess(settle, arm)
        for ch in delays:
            self.assertLess(events.index(("state", ch, True)), settle)
        self.assertEqual(events[arm + 1 : arm + 3], [("cycles", 0, 3), "read"])
        self.assertEqual(events.count(("cycles", 0, 3)), 2)
        self.assertEqual(ts.windows, [1.0])

    def test_group_covers_all_channels_counts_modes_and_boundaries(self):
        events = []
        ts, pg = FakeTimestamper(events, []), FakePulsegen(events)
        offsets = {0: 0, 1: 0, 2: 0, 3: -16}
        with patch.object(regression, "prime_gp_prescalers"), patch.object(
            regression, "run_gp_burst_boundary_case", return_value=True
        ) as run, patch.object(
            regression, "measure_gp_sync_offsets", return_value=offsets
        ) as calibrate, patch(
            "sys.stdout", new=io.StringIO()
        ):
            self.assertTrue(regression.test_gp_burst_boundary(ts, pg, 0.1))
        calibrate.assert_called_once_with(ts, pg, 750_000, 0.1)
        self.assertEqual(run.call_count, 18)
        for count in (1, 3):
            for ch in range(4):
                for delay in (0, 990_000):
                    run.assert_any_call(
                        ts, pg, regression.Pulsegen.ASYNC, {ch: delay}, 10_000, count, 0.1
                    )
            run.assert_any_call(
                ts,
                pg,
                regression.Pulsegen.SYNC,
                dict(enumerate((0, 100_000, 200_000, 250_000))),
                750_000,
                count,
                0.1,
                phase_offsets_ns=offsets,
            )
        self.assertIs(regression.TESTS["gp-burst-boundary"], regression.test_gp_burst_boundary)


class WidthTests(unittest.TestCase):
    def test_only_true_window_orphans_are_allowed(self):
        records = [(0, 0, "-"), *pulses([100, 200], 10), (0, 300, "+")]
        self.assertEqual(regression.pulse_widths(records, 0), [10, 10])
        with self.assertRaisesRegex(RuntimeError, "preceding rising"):
            regression.pulse_widths(records, 0, allow_leading_fall=False)

    def test_missing_interior_fall_is_not_silently_repaired(self):
        records = [(0, 0, "+"), (0, 10, "-"), (0, 100, "+"), (0, 200, "+"), (0, 210, "-")]
        with self.assertRaisesRegex(RuntimeError, "two rising"):
            regression.pulse_widths(records, 0)

    def test_extra_interior_fall_fails(self):
        with self.assertRaisesRegex(RuntimeError, "preceding rising"):
            regression.pulse_widths([(0, 0, "+"), (0, 10, "-"), (0, 20, "-")], 0)

    def test_duplicate_and_zero_width_edges_fail(self):
        for records in ([(0, 0, "+"), (0, 0, "+")], [(0, 0, "+"), (0, 0, "-")]):
            with self.assertRaisesRegex(RuntimeError, "coincident"):
                regression.pulse_widths(records, 0)

    def test_independently_batched_polarities_are_sorted(self):
        records = pulses([0, 100, 200], 10)
        records = records[::2] + records[1::2]
        self.assertEqual(regression.pulse_widths(records, 0), [10, 10, 10])

    def test_asymmetric_steady_capture_boundaries_use_common_coverage(self):
        period, width = 1_000_000, 10_000
        complete = pulses([i * period for i in range(40)], width)
        for missing_polarity in ("+", "-"):
            with self.subTest(missing_polarity=missing_polarity):
                records = [
                    rec
                    for rec in complete
                    if rec[2] != missing_polarity or 8 * period <= rec[1] < 30 * period
                ]
                # Match actual transport order: separate, unequal polarity batches.
                records.sort(key=lambda rec: rec[2])
                widths = regression.pulse_widths(records, 0, period_ns=period)
                self.assertEqual(widths, [width] * 22)
                with patch("sys.stdout", new=io.StringIO()):
                    self.assertTrue(regression.check_gp_pulses(records, 0, period, width))

    def test_startup_trims_only_asymmetric_tail(self):
        period, width = 1_000_000, 250_000
        complete = pulses([i * period for i in range(40)], width)
        for missing_polarity in ("+", "-"):
            with self.subTest(missing_polarity=missing_polarity):
                records = [
                    rec for rec in complete if rec[2] != missing_polarity or rec[1] < 30 * period
                ]
                self.assertEqual(
                    regression.pulse_widths(records, 0, allow_leading_fall=False, period_ns=period),
                    [width] * 30,
                )
                with patch("sys.stdout", new=io.StringIO()):
                    self.assertTrue(
                        regression.check_gp_pulses(records, 0, period, width, startup=True)
                    )

    def test_startup_never_crops_missing_first_polarities(self):
        period, width = 1_000_000, 250_000
        complete = pulses([i * period for i in range(40)], width)
        for missing_polarity in ("+", "-"):
            with self.subTest(missing_polarity=missing_polarity):
                records = [
                    rec for rec in complete if rec[2] != missing_polarity or rec[1] >= 8 * period
                ]
                with self.assertRaisesRegex(RuntimeError, "preceding rising|two rising"):
                    regression.check_gp_pulses(records, 0, period, width, startup=True)

    def test_common_coverage_does_not_repair_interior_faults(self):
        period, width = 1_000_000, 10_000
        complete = pulses([i * period for i in range(40)], width)
        asymmetric = [rec for rec in complete if rec[2] == "+" or rec[1] < 30 * period]
        missing_fall = [rec for rec in asymmetric if rec != (0, 10 * period + width, "-")]
        extra_fall = [*asymmetric, (0, 10 * period + width + 4, "-")]
        duplicate = [*asymmetric, asymmetric[0]]
        for records in (missing_fall, extra_fall, duplicate):
            with self.subTest(records=records):
                with self.assertRaises(RuntimeError):
                    regression.check_gp_pulses(records, 0, period, width)

    def test_coverage_skew_is_bounded(self):
        period, width = 1_000_000, 10_000
        complete = pulses([i * period for i in range(350)], width)
        records = [rec for rec in complete if rec[2] == "+" or rec[1] < 30 * period]
        with self.assertRaisesRegex(RuntimeError, "more than one period"):
            regression.check_gp_pulses(records, 0, period, width)

    def test_long_period_capture_can_end_between_edges(self):
        period, width = regression.NS, regression.NS // 4
        for startup, count in ((True, 3), (False, 20)):
            for polarity, offset in (("+", 0), ("-", width)):
                with self.subTest(startup=startup, polarity=polarity):
                    records = pulses([i * period for i in range(count)], width)
                    records.append((0, count * period + offset, polarity))
                    self.assertEqual(
                        regression.pulse_widths(
                            records, 0, allow_leading_fall=not startup, period_ns=period
                        ),
                        [width] * count,
                    )
                    with patch("sys.stdout", new=io.StringIO()):
                        self.assertTrue(
                            regression.check_gp_pulses(records, 0, period, width, startup=startup)
                        )

    def test_long_period_tail_still_has_a_finite_bound(self):
        period, width = regression.NS, regression.NS // 4
        records = pulses([0, period, 2 * period], width)
        records += [(0, 3 * period, "+"), (0, 4 * period, "+")]
        with self.assertRaisesRegex(RuntimeError, "more than one period"):
            regression.check_gp_pulses(records, 0, period, width, startup=True)

    def test_long_period_tail_does_not_hide_first_or_interior_faults(self):
        period, width = regression.NS, regression.NS // 4
        missing_fall = pulses([0, period, 2 * period], width)
        missing_fall.remove((0, period + width, "-"))
        missing_fall.append((0, 3 * period, "+"))
        with self.assertRaisesRegex(RuntimeError, "two rising"):
            regression.check_gp_pulses(missing_fall, 0, period, width, startup=True)
        short_width = pulses([0], width // 2) + pulses([period, 2 * period], width)
        short_width.append((0, 3 * period, "+"))
        short_gap = pulses([0, period // 2, 3 * period // 2], width)
        short_gap.append((0, 5 * period // 2, "+"))
        for records in (short_width, short_gap):
            with self.subTest(records=records), patch("sys.stdout", new=io.StringIO()):
                self.assertFalse(
                    regression.check_gp_pulses(records, 0, period, width, startup=True)
                )

    def test_failure_reports_only_first_four_gaps_and_widths(self):
        records = pulses([0], 10) + pulses([i * 1000 for i in range(1, 7)], 100)
        with patch("sys.stdout", new=io.StringIO()) as output:
            self.assertFalse(regression.check_gp_pulses(records, 0, 1000, 100, startup=True))
        diagnostics = output.getvalue().splitlines()[1]
        self.assertEqual(
            diagnostics.strip(),
            "first gaps (ns): [1000, 1000, 1000, 1000]; first widths (ns): [10, 100, 100, 100]",
        )

    def test_wholly_missing_polarity_and_no_overlap_fail(self):
        rises = [(0, i * 1_000_000, "+") for i in range(40)]
        with self.assertRaisesRegex(RuntimeError, "missing rising or falling"):
            regression.check_gp_pulses(rises, 0, 1_000_000, 10_000)
        later_falls = [(0, i * 1_000_000 + 10_000, "-") for i in range(100, 140)]
        with self.assertRaisesRegex(RuntimeError, "no common"):
            regression.check_gp_pulses(rises + later_falls, 0, 1_000_000, 10_000)

    def test_startup_first_fault_survives_tail_crop(self):
        period, width = 1_000_000, 250_000
        starts = [0] + [i * period - period // 2 for i in range(1, 40)]
        short_gap = pulses(starts, width)
        short_width = pulses([0], width // 2) + pulses([i * period for i in range(1, 40)], width)
        for records in (short_gap, short_width):
            with self.subTest(records=records), patch("sys.stdout", new=io.StringIO()):
                records = [rec for rec in records if rec[2] == "+" or rec[1] < 30 * period]
                self.assertFalse(
                    regression.check_gp_pulses(records, 0, period, width, startup=True)
                )

    def test_first_short_interval_is_not_trimmed(self):
        with patch("sys.stdout", new=io.StringIO()):
            self.assertFalse(
                regression.check_gp_pulses(
                    pulses([0, 500, 1500, 2500], 250), 0, 1000, 250, startup=True
                )
            )

    def test_first_bad_width_is_not_hidden_by_later_good_widths(self):
        records = pulses([0], 100) + pulses([1000, 2000, 3000], 250)
        with patch("sys.stdout", new=io.StringIO()):
            self.assertFalse(regression.check_gp_pulses(records, 0, 1000, 250, startup=True))

    def test_guard_clamp_including_one_timer_tick_fails(self):
        starts = [i * regression.GP_WRAP_PERIOD_NS for i in range(20)]
        with patch("sys.stdout", new=io.StringIO()):
            for width in (3984, 9984):
                self.assertFalse(
                    regression.check_gp_pulses(
                        pulses(starts, width), 0, regression.GP_WRAP_PERIOD_NS, 10_000
                    )
                )
            self.assertTrue(
                regression.check_gp_pulses(
                    pulses(starts, 10_000), 0, regression.GP_WRAP_PERIOD_NS, 10_000
                )
            )

    def test_empty_capture_cannot_pass(self):
        with self.assertRaisesRegex(RuntimeError, "too few"):
            regression.check_gp_pulses([], 0, 1000, 250, startup=True)


class RepetitionTests(unittest.TestCase):
    def test_half_length_initial_interval_stays_in_separate_bursts(self):
        records = bursts([0, 20 * regression.NS])
        sizes, spacing, gaps = regression.repetition_metrics(records)
        self.assertEqual(sizes, [3, 3])
        self.assertEqual(spacing, 0)
        self.assertEqual(gaps, [20 * regression.NS])
        with patch("sys.stdout", new=io.StringIO()):
            self.assertFalse(regression.check_repetition_startup(records, 40))

    def test_double_length_initial_interval_is_not_trimmed(self):
        records = bursts([0, 200_000_000, 300_000_000, 400_000_000])
        with patch("sys.stdout", new=io.StringIO()):
            self.assertFalse(regression.check_repetition_startup(records, 0.1))

    def test_bad_first_burst_count_is_not_trimmed(self):
        records = bursts([0], ncyc=2) + bursts([100_000_000, 200_000_000])
        with patch("sys.stdout", new=io.StringIO()):
            self.assertFalse(regression.check_repetition_startup(records, 0.1))

    def test_correct_first_and_later_intervals_pass(self):
        with patch("sys.stdout", new=io.StringIO()):
            self.assertTrue(
                regression.check_repetition_startup(bursts([0, 40 * regression.NS]), 40)
            )
            self.assertTrue(
                regression.check_repetition_startup(bursts([0, 100_000_000, 200_000_000]), 0.1)
            )

    def test_one_burst_is_not_enough_to_prime_reverse_transition(self):
        with self.assertRaisesRegex(RuntimeError, "at least two"):
            regression.check_repetition_startup(bursts([0]), 40)


if __name__ == "__main__":
    unittest.main()
