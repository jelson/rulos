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


class QueryBacklogTests(unittest.TestCase):
    def test_backlog_phase_checks_replies_load_and_stream_state(self):
        for wire in regression.WIRES:
            for defect in (None, "reply", "no backlog", "no resume", "not silent"):
                with self.subTest(wire=wire, defect=defect):
                    tic = Mock()
                    tic._ser.in_waiting = 0 if defect == "no backlog" else 4096
                    tic.idn.return_value = "Lectrobox,LectroTIC-4,test,version"
                    tic._stream_on = False
                    tic.set_stream_enabled.side_effect = lambda on: setattr(tic, "_stream_on", on)
                    responses = {
                        "*IDN?": tic.idn.return_value,
                        "FORM:DATA?": "TEXT" if wire == "text" else "BIN",
                        "INP1:DIV?": "1",
                        "OUTP:STAT?": "0",
                    }
                    tic.query.side_effect = lambda command: (
                        "timestamp fragment" if defect == "reply" else responses[command]
                    )
                    tic.read_raw.side_effect = lambda duration: (
                        b"stream data"
                        if (tic._stream_on and defect != "no resume") or defect == "not silent"
                        else b""
                    )
                    ctx = SimpleNamespace(
                        tic=tic, src=Mock(), ph=tstest.Phase(), channel=1, wire=wire
                    )
                    with patch.object(regression.time, "sleep"), redirect_stdout(io.StringIO()):
                        regression.phase_query_backlog(ctx)
                    self.assertEqual(ctx.ph.ok, defect is None)
                    self.assertEqual(tic.query.call_count, 9)
                    self.assertEqual(tic.discard_pending.call_count, 2)
                    calls = [call[0] for call in tic.method_calls]
                    first, last = calls.index("query"), len(calls) - 1 - calls[::-1].index("query")
                    self.assertNotIn("discard_pending", calls[first : last + 1])
                    self.assertFalse(tic._stream_on)
                    self.assertEqual(ctx.src.off.call_count, 2)

    def test_backlog_phase_registered_for_each_channel_and_wire(self):
        phases = [pd for pd in regression.PHASES if pd.fn is regression.phase_query_backlog]
        self.assertEqual(len(phases), 1)
        self.assertTrue(phases[0].per_channel)
        self.assertEqual(phases[0].wires, regression.WIRES)


def finite_both_capture(pulses, divider, progress=0, channel=1, wire="binary"):
    records = []
    for cycle in range(pulses):
        for offset, polarity in ((0, "+"), (regression.FINITE_WIDTH_NS, "-")):
            progress += 1
            if progress == divider:
                progress = 0
                records.append(
                    (10 * tstest.NS + cycle * regression.FINITE_SPACING_NS + offset, polarity)
                )
    return tstest.Capture(wire, chans={channel: records})


class FiniteBothDividerTests(unittest.TestCase):
    def score(self, cap, pulses, divider, progress=0):
        ph = tstest.Phase()
        with redirect_stdout(io.StringIO()):
            regression._expect_finite_both_divider(ph, cap, 1, pulses, divider, progress)
        return ph.ok

    def test_correct_finite_selection_and_carry_accept_batched_wire(self):
        for divider in (3, 5):
            for pulses, carry in ((1031, 0), (2, 2062 % divider)):
                cap = finite_both_capture(pulses, divider, carry)
                cap.chans[1] = polarity_batches(cap.chans[1])
                with self.subTest(divider=divider, pulses=pulses, carry=carry):
                    self.assertTrue(self.score(cap, pulses, divider, carry))

    def test_no_missing_duplicate_or_shifted_finite_edge_is_trimmed(self):
        for divider in (3, 5):
            for index in (0, 100, -1):
                for defect in ("missing", "duplicate", "polarity", "timing"):
                    cap = finite_both_capture(1031, divider)
                    stamp, pol = cap.chans[1][index]
                    if defect == "missing":
                        del cap.chans[1][index]
                    elif defect == "duplicate":
                        cap.chans[1].insert(index, (stamp, pol))
                    elif defect == "polarity":
                        cap.chans[1][index] = (stamp, "-" if pol == "+" else "+")
                    else:
                        cap.chans[1][index] = (stamp + 16, pol)
                    with self.subTest(divider=divider, index=index, defect=defect):
                        self.assertFalse(self.score(cap, 1031, divider))

    def test_first_frame_batch_order_and_lost_continuation_carry_fail(self):
        for divider in (3, 5):
            raw = finite_both_capture(1031, 1).chans[1]
            selected_in_batch_order = polarity_batches(raw)[divider - 1 :: divider]
            cap = tstest.Capture("binary", chans={1: selected_in_batch_order})
            with self.subTest(divider=divider, defect="batch order"):
                self.assertFalse(self.score(cap, 1031, divider))
            with self.subTest(divider=divider, defect="lost carry"):
                self.assertFalse(
                    self.score(finite_both_capture(2, divider), 2, divider, 2062 % divider)
                )

    def test_loss_wrong_channel_and_backwards_substream_fail(self):
        for defect in ("overcapture", "overflow", "other channel", "backwards"):
            cap = finite_both_capture(1031, 3)
            if defect == "other channel":
                cap.chans[2] = [cap.chans[1][0]]
            elif defect == "backwards":
                cap.chans[1][100], cap.chans[1][102] = cap.chans[1][102], cap.chans[1][100]
            else:
                cap.loss[1] = [int(defect == "overcapture"), int(defect == "overflow")]
            with self.subTest(defect=defect):
                self.assertFalse(self.score(cap, 1031, 3))

    def test_capture_quantization_is_accepted(self):
        cap = finite_both_capture(1031, 3)
        cap.chans[1] = [
            (stamp + (4 if i % 2 else -4), pol) for i, (stamp, pol) in enumerate(cap.chans[1])
        ]
        self.assertTrue(self.score(cap, 1031, 3))

    def test_finite_phase_has_one_initial_clear_and_no_carry_reset(self):
        for divider in (3, 5):
            for wire in regression.WIRES:
                ctx = SimpleNamespace(
                    ph=tstest.Phase(), src=Mock(), tic=Mock(), channel=1, wire=wire
                )
                timeline = Mock()
                timeline.attach_mock(ctx.src, "src")
                timeline.attach_mock(ctx.tic, "tic")
                ctx.src.burst.return_value = regression.FINITE_SPACING_NS
                captures = [
                    finite_both_capture(1031, divider, wire=wire),
                    finite_both_capture(2, divider, 2062 % divider, wire=wire),
                ]
                with patch.object(tstest, "capture", side_effect=captures) as capture, patch.object(
                    regression.time, "sleep"
                ), redirect_stdout(io.StringIO()):
                    timeline.attach_mock(capture, "capture")
                    regression.phase_both_divider_burst(ctx, divider)
                self.assertTrue(ctx.ph.ok)
                calls = timeline.mock_calls
                bursts = [i for i, c in enumerate(calls) if c[0] == "src.burst"]
                self.assertEqual([calls[i].args[2] for i in bursts], [1031, 2])
                self.assertEqual(ctx.tic.discard_pending.call_count, 1)
                for i, call in enumerate(calls):
                    if call[0] in (
                        "tic.discard_pending",
                        "tic.send",
                        "tic.set_divider",
                        "tic.set_slope",
                    ):
                        self.assertLess(i, bursts[0], "input or carry was cleared between frames")
                    if call[0] == "capture":
                        self.assertIs(call.args[0], ctx.tic)
                        self.assertEqual(call.args[1], wire)
                        self.assertEqual(call.kwargs, {"discard": False})
                for burst in bursts:
                    end = next(i for i in range(burst + 1, len(calls)) if calls[i][0] == "capture")
                    self.assertTrue(
                        any(
                            c[0] == "src.pg.set_state" and c.args == (1, False)
                            for c in calls[burst:end]
                        )
                    )

    def test_new_phases_are_per_channel_and_per_wire(self):
        phases = [
            pd
            for pd in regression.PHASES
            if pd.fn in (regression.phase_both_divider_burst, regression.phase_both_divider_sparse)
        ]
        self.assertEqual(len(phases), 3)
        self.assertTrue(all(pd.per_channel and pd.wires == regression.WIRES for pd in phases))


class SparseBothDividerTests(unittest.TestCase):
    def run_phase(self, wire, defect=None):
        ctx = SimpleNamespace(ph=tstest.Phase(), src=Mock(), tic=Mock(), channel=1, wire=wire)
        ctx.src.burst.return_value = regression.FINITE_SPACING_NS
        timeline = Mock()
        timeline.attach_mock(ctx.src, "src")
        timeline.attach_mock(ctx.tic, "tic")
        now = 0.0
        captured = 0

        def sleep(delay):
            nonlocal now
            now += delay

        def check(context):
            if context == "starting sparse BOTH pulse" and defect == "late enable":
                sleep(0.2)

        def capture(tic, got_wire, duration, discard=True):
            nonlocal captured
            self.assertIs(tic, ctx.tic)
            self.assertEqual(got_wire, wire)
            self.assertFalse(discard, "sparse reads must preserve divider carry")
            sleep(duration)
            if captured == 0:
                records = [(20 * tstest.NS, "+")]
                if defect == "held rise":
                    records = []
                elif defect == "extra pulse":
                    records *= 2
                elif defect == "wrong polarity":
                    records = [(20 * tstest.NS, "-")]
                elif defect == "late read":
                    sleep(0.2)
            elif captured == 1:
                records = [(20 * tstest.NS, "+")] if defect == "held rise" else []
            else:
                records = [] if defect == "lost fall carry" else [(22 * tstest.NS, "-")]
            captured += 1
            cap = tstest.Capture(wire, chans={1: records})
            if defect == "loss":
                cap.loss[1] = [0, 1]
            elif defect == "other channel":
                cap.chans[2] = [(20 * tstest.NS, "+")]
            return cap

        ctx.src._check.side_effect = check
        with patch.object(regression.time, "sleep", side_effect=sleep), patch.object(
            regression.time, "monotonic", side_effect=lambda: now
        ), patch.object(tstest, "capture", side_effect=capture) as mocked_capture, redirect_stdout(
            io.StringIO()
        ):
            timeline.attach_mock(mocked_capture, "capture")
            regression.phase_both_divider_sparse(ctx)
        return ctx, timeline.mock_calls

    def test_sparse_tail_is_delivered_without_waiting_for_fall(self):
        for wire in regression.WIRES:
            with self.subTest(wire=wire):
                ctx, _ = self.run_phase(wire)
                self.assertTrue(ctx.ph.ok)

    def test_held_tail_wrong_carry_timing_loss_and_extra_edges_fail(self):
        for wire in regression.WIRES:
            for defect in (
                "held rise",
                "extra pulse",
                "wrong polarity",
                "lost fall carry",
                "late read",
                "late enable",
                "loss",
                "other channel",
            ):
                with self.subTest(wire=wire, defect=defect):
                    ctx, _ = self.run_phase(wire, defect)
                    self.assertFalse(ctx.ph.ok)
                    self.assertEqual(ctx.src.pg.set_state.call_args.args, (1, False))

    def test_sparse_staging_and_reads_cannot_clear_the_primed_carry(self):
        ctx, calls = self.run_phase("binary")
        bursts = [i for i, c in enumerate(calls) if c[0] == "src.burst"]
        self.assertEqual([calls[i].args[2] for i in bursts], [1, 1])
        enabled = [
            i for i, c in enumerate(calls) if c[0] == "src.pg.set_state" and c.args == (1, True)
        ]
        self.assertEqual(len(enabled), 1)
        self.assertLess(bursts[0], enabled[0])
        self.assertLess(enabled[0], bursts[1])
        self.assertEqual(ctx.tic.discard_pending.call_count, 1)
        for i, call in enumerate(calls):
            if call[0] in ("tic.discard_pending", "tic.send", "tic.set_divider", "tic.set_slope"):
                self.assertLess(i, bursts[0])
            elif call[0] in ("src.pg.set_period", "src.pg.set_width", "src.pg.set_delay"):
                self.assertLess(i, enabled[0], "source timing reconfigured after capture started")
        self.assertEqual(ctx.src.pg.set_width.call_args.args, (1, 0.75))


def ring_backlog(channel=1):
    prefix = [(i * regression.FINITE_SPACING_NS, "+") for i in range(regression.RING_PREFIX_PULSES)]
    probe = [
        (tstest.NS + (3 * i + 2) * regression.FINITE_SPACING_NS, "+")
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
            regression._finite_input_burst(ctx, 16000, 0.75)
        ctx.src.pg.set_state.assert_called_once_with(3, False)

    def test_source_configuration_failure_still_disables_active_channel(self):
        ctx = SimpleNamespace(src=Mock(), channel=2)
        ctx.src.burst.side_effect = RuntimeError("configuration failed")
        with self.assertRaisesRegex(RuntimeError, "configuration failed"):
            regression._finite_input_burst(ctx, 16000, 0.75)
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
