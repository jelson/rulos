#!/usr/bin/env python3
"""Hardware-free checks that paired-regression scoring rejects incorrect edge selections."""

from contextlib import redirect_stdout
import io
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

import regression_test as regression
import tstest


def source_edges(pulses=4000):
    return [
        (cycle * regression.PERIOD_NS + offset, polarity)
        for cycle in range(pulses)
        for offset, polarity in ((0, "+"), (regression.PULSE_WIDTH_NS, "-"))
    ]


def selected_edges(divider, phase=0):
    return source_edges()[divider - 1 + phase :: divider]


def polarity_batches(records):
    """The wire may emit one polarity before the other inside each 100 ms flush window."""
    return sorted(records, key=lambda rec: (rec[0] // 100_000_000, rec[1] != "+", rec[0]))


def old_batch_divider(divider):
    """Original failure: count all available rising captures before the falling captures."""
    return polarity_batches(source_edges())[divider - 1 :: divider]


class BothDividerTests(unittest.TestCase):
    def score(self, records, divider):
        ph = tstest.Phase()
        with redirect_stdout(io.StringIO()):
            regression._expect_both_divider(
                ph, records, divider, regression.PERIOD_NS, regression.PULSE_WIDTH_NS
            )
        return ph.ok

    def test_correct_selection_accepts_batched_wire_and_either_starting_edge(self):
        for divider in (2, 3, 5):
            for start in (0, 1):
                with self.subTest(divider=divider, starting_edge=start):
                    self.assertTrue(
                        self.score(polarity_batches(selected_edges(divider, start)), divider)
                    )

    def test_original_batch_order_selection_is_rejected(self):
        for divider in (2, 3, 5):
            with self.subTest(divider=divider):
                records = old_batch_divider(divider)
                self.assertEqual(len(records), len(selected_edges(divider)))
                self.assertFalse(self.score(records, divider))

    def test_missing_duplicate_timing_and_polarity_defects_are_rejected(self):
        for divider in (2, 3, 5):
            clean = selected_edges(divider)
            middle = len(clean) // 2
            missing = clean[:middle] + clean[middle + 1 :]
            duplicate = clean[:middle] + [clean[middle]] + clean[middle:]
            timing = list(clean)
            timing[middle] = (timing[middle][0] + 16, timing[middle][1])
            polarity = list(clean)
            stamp, pol = polarity[middle]
            polarity[middle] = (stamp, "-" if pol == "+" else "+")
            for name, records in (
                ("missing", missing),
                ("duplicate", duplicate),
                ("timing", timing),
                ("polarity", polarity),
            ):
                with self.subTest(divider=divider, defect=name):
                    self.assertFalse(self.score(polarity_batches(records), divider))

    def test_backwards_substream_is_not_hidden_by_host_sorting(self):
        for divider in (2, 3, 5):
            records = selected_edges(divider)
            middle = len(records) // 2
            other = middle + (1 if divider % 2 == 0 else 2)
            records[middle], records[other] = records[other], records[middle]
            with self.subTest(divider=divider):
                self.assertFalse(self.score(records, divider))

    def test_capture_quantization_is_accepted(self):
        for divider in (2, 3, 5):
            records = [
                (stamp + (4 if i % 2 else -4), polarity)
                for i, (stamp, polarity) in enumerate(selected_edges(divider))
            ]
            with self.subTest(divider=divider):
                self.assertTrue(self.score(polarity_batches(records), divider))

    def test_empty_and_short_traces_fail(self):
        for records in ([], selected_edges(3)[:3]):
            self.assertFalse(self.score(records, 3))

    def test_registered_for_every_channel_and_wire(self):
        phases = [pd for pd in regression.PHASES if pd.fn is regression.phase_both_divider]
        self.assertEqual([pd.params["divider"] for pd in phases], [2, 3, 5])
        self.assertTrue(all(pd.per_channel and pd.wires == regression.WIRES for pd in phases))

    def test_phase_rejects_loss_wrong_channel_and_missing_load(self):
        for defect in (None, "loss", "other channel", "empty"):
            cap = tstest.Capture("binary", chans={1: polarity_batches(selected_edges(3))})
            if defect == "loss":
                cap.loss[1] = [0, 1]
            elif defect == "other channel":
                cap.chans[2] = [cap.chans[1][0]]
            elif defect == "empty":
                cap.chans.clear()
            ctx = SimpleNamespace(
                ph=tstest.Phase(), src=Mock(), tic=Mock(), channel=1, wire="binary", duration=2.0
            )
            with self.subTest(defect=defect), patch.object(
                tstest, "capture", return_value=cap
            ), redirect_stdout(io.StringIO()):
                regression.phase_both_divider(ctx, 3)
            self.assertEqual(ctx.ph.ok, defect is None)


def ring_backlog(channel=1):
    prefix = [(i * regression.RING_SPACING_NS, "+") for i in range(regression.RING_PREFIX_PULSES)]
    probe = [
        (tstest.NS + (3 * i + 2) * regression.RING_SPACING_NS, "+")
        for i in range(regression.RING_PROBE_PULSES // regression.RING_PROBE_DIVIDER)
    ]
    return tstest.Capture("binary", chans={channel: prefix + probe})


class RingPressureTests(unittest.TestCase):
    def score(self, cap):
        ph = tstest.Phase()
        with redirect_stdout(io.StringIO()):
            regression._expect_ring_backlog(ph, cap, 1)
        return ph.ok

    def test_complete_backlog_passes(self):
        self.assertTrue(self.score(ring_backlog()))

    def test_missing_prefix_and_probe_records_fail(self):
        for index in (500, regression.RING_PREFIX_PULSES + 100):
            cap = ring_backlog()
            del cap.chans[1][index]
            with self.subTest(index=index):
                self.assertFalse(self.score(cap))

    def test_missing_or_empty_preload_cannot_pass(self):
        for records in ([], ring_backlog().chans[1][regression.RING_PREFIX_PULSES :]):
            with self.subTest(count=len(records)):
                self.assertFalse(self.score(tstest.Capture("binary", chans={1: records})))

    def test_duplicate_and_off_grid_records_fail(self):
        for index in (500, regression.RING_PREFIX_PULSES + 100):
            for duplicate in (False, True):
                cap = ring_backlog()
                stamp, pol = cap.chans[1][index]
                if duplicate:
                    cap.chans[1].insert(index, (stamp, pol))
                else:
                    cap.chans[1][index] = (stamp + 16, pol)
                with self.subTest(index=index, duplicate=duplicate):
                    self.assertFalse(self.score(cap))

    def test_loss_and_unexpected_channel_fail(self):
        for defect in ("overcapture", "ring overflow", "other channel", "polarity"):
            cap = ring_backlog()
            if defect == "other channel":
                cap.chans[2] = [(0, "+")]
            elif defect == "polarity":
                stamp, _ = cap.chans[1][500]
                cap.chans[1][500] = (stamp, "-")
            else:
                cap.loss[1] = [int(defect == "overcapture"), int(defect == "ring overflow")]
            with self.subTest(defect=defect):
                self.assertFalse(self.score(cap))

    def test_phase_preserves_backlog_and_divider_progress(self):
        for wire in regression.WIRES:
            for continuation_count in (0, 1, 2):
                events = []
                stage = 0
                stream_on = True
                ctx = SimpleNamespace(
                    ph=tstest.Phase(), src=Mock(), tic=Mock(), channel=3, wire=wire
                )

                def require_initial(*args, **kwargs):
                    self.assertEqual(stage, 0, "configuration or clear erased a live backlog")

                def divider(channel, value):
                    if stage:
                        self.assertEqual((stage, channel, value), (1, 3, 3))
                    events.append(("divider", channel, value))

                def stream(enabled):
                    nonlocal stream_on
                    if stage < 2:
                        self.assertFalse(enabled, "output enabled before the preload and probe")
                    stream_on = enabled

                def burst(channel, spacing, ncyc, rep_s):
                    nonlocal stage
                    self.assertEqual((channel, spacing, rep_s), (3, 1000, 60.0))
                    self.assertEqual(ncyc, (16000, 1148, 1)[stage])
                    self.assertEqual(stream_on, stage == 2)
                    if stage:
                        self.assertIn(("off", stage, 3), events)
                    stage += 1
                    events.append(("burst", stage))
                    return spacing

                def disable(channel, enabled):
                    self.assertEqual((channel, enabled), (3, False))
                    events.append(("off", stage, channel))

                def capture(tic, got_wire, duration, discard=True):
                    self.assertIs(tic, ctx.tic)
                    self.assertEqual(got_wire, wire)
                    self.assertFalse(discard, "capture must not marker-sync away the backlog")
                    self.assertTrue(stream_on)
                    self.assertIn(("off", stage, 3), events)
                    if stage == 2:
                        events.append(("drained",))
                        return ring_backlog(3)
                    self.assertEqual(stage, 3)
                    self.assertIn(("drained",), events)
                    return tstest.Capture(
                        wire, chans={3: [(2 * tstest.NS, "+")] * continuation_count}
                    )

                ctx.tic.send.side_effect = require_initial
                ctx.tic.discard_pending.side_effect = require_initial
                ctx.tic.reset.side_effect = require_initial
                ctx.tic.read_for.side_effect = AssertionError("read_for clears the backlog")
                ctx.tic.set_slope.side_effect = require_initial
                ctx.tic.set_serial_enabled.side_effect = require_initial
                ctx.tic.set_divider.side_effect = divider
                ctx.tic.set_stream_enabled.side_effect = stream
                ctx.src.off.side_effect = require_initial
                ctx.src.burst.side_effect = burst
                ctx.src.pg.set_state.side_effect = disable
                with self.subTest(wire=wire, continuation_count=continuation_count), patch.object(
                    tstest, "capture", side_effect=capture
                ), patch.object(regression.time, "sleep"), redirect_stdout(io.StringIO()):
                    regression.phase_divider_ring_pressure(ctx)
                self.assertEqual(ctx.ph.ok, continuation_count == 1)

    def test_source_deadline_failure_still_disables_active_channel(self):
        ctx = SimpleNamespace(src=Mock(), channel=3)
        ctx.src.burst.return_value = 1000
        with patch.object(regression.time, "sleep"), patch.object(
            regression.time, "monotonic", side_effect=[0, 5]
        ), self.assertRaisesRegex(RuntimeError, "deadline exceeded"):
            regression._ring_input_burst(ctx, 16000, 0.75)
        ctx.src.pg.set_state.assert_called_once_with(3, False)

    def test_source_configuration_failure_still_disables_active_channel(self):
        ctx = SimpleNamespace(src=Mock(), channel=2)
        ctx.src.burst.side_effect = RuntimeError("configuration failed")
        with self.assertRaisesRegex(RuntimeError, "configuration failed"):
            regression._ring_input_burst(ctx, 16000, 0.75)
        ctx.src.pg.set_state.assert_called_once_with(2, False)

    def test_source_burst_retains_default_and_accepts_long_repetition(self):
        pg = Mock()
        pg.get_error.return_value = '0,"No error"'
        pg.pulse_burst.return_value = 1000
        source = tstest.Source(pg)
        for rep in (None, 60.0):
            with self.subTest(rep_s=rep):
                if rep is None:
                    actual = source.burst(2, 1000, 16000)
                else:
                    actual = source.burst(2, 1000, 16000, rep_s=rep)
                self.assertEqual(actual, 1000)
                pg.pulse_burst.assert_called_with(2, 1000, 16000, rep_s=rep or 1.0)


if __name__ == "__main__":
    unittest.main()
