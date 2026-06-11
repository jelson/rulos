"""Shared library for the LectroTIC-4 hardware test suite.

One capture model, one scoring vocabulary, one source fixture -- every test states its intent in
these terms instead of hand-rolling decode/score/setup. See REFACTOR_PLAN.md for the suite design.

Principles baked in here so individual tests can't drift from them:

- Integer nanoseconds end to end. A float64 second loses nanosecond
  precision after ~104 days of device uptime (the same hazard the
  product docs warn users about), so no timestamp ever touches a
  float.

- Zero tolerance. Scoring helpers fail on any missing, duplicated, or
  misplaced record; decode raises on any corrupt byte. The only
  structural allowances are capture-window edges, owned by the helpers
  that need them and documented where they live.

- Known-state configuration. Source (PG-4) and device fixtures fully
  specify every state they touch, so no test inherits a prior test's
  leftovers."""

import re
import statistics
import time
from dataclasses import dataclass, field

from pgctl import Pulsegen
import tsctl
from tsctl import Timestamp, PulsesLost, OscillatorFailure

NS = 1_000_000_000

# The capture counter ticks at 250 MHz: all timestamps quantize to a 4 ns grid.
TICK_NS = 4

# Gap scoring tolerance: one tick of capture quantization on each of the two edges forming a gap,
# plus one tick of source placement (PG-4 HRTIM quantization) = 3 ticks. A real missed pulse
# deviates by a whole period -- orders of magnitude past this -- so the tolerance distinguishes
# measurement granularity from misbehavior with no gray zone.
GAP_TOL_NS = 12

CHANNELS = (0, 1, 2, 3)

_DIAG_RE = re.compile(r"# ch(\d+): (\d+) overcaptures, (\d+) buf overflows")


# --------------------------------------------------------------------
# The record model
# --------------------------------------------------------------------


@dataclass
class Capture:
    """Everything one capture window produced: per-channel records in arrival order (each (t_ns,
    polarity)), the device's in-band loss counts attributed per channel, and any '#' comment lines
    (text wire only)."""

    wire: str
    chans: dict = field(default_factory=dict)
    loss: dict = field(default_factory=dict)  # channel -> [overcaptures, buf_overflows]
    comments: list = field(default_factory=list)

    @property
    def overcaptures(self):
        return sum(v[0] for v in self.loss.values())

    @property
    def buf_overflows(self):
        return sum(v[1] for v in self.loss.values())

    def loss_for(self, ch):
        return tuple(self.loss.get(ch, (0, 0)))

    def _add_loss(self, ch, oc, ovf):
        entry = self.loss.setdefault(ch, [0, 0])
        entry[0] += oc
        entry[1] += ovf

    def records(self, ch):
        return self.chans.get(ch, [])

    def times(self, ch):
        return [t for t, _ in self.records(ch)]

    def pols(self, ch):
        return [p for _, p in self.records(ch)]

    def channels(self):
        return {c for c, recs in self.chans.items() if recs}

    def _add(self, ch, t_ns, pol):
        self.chans.setdefault(ch, []).append((t_ns, pol))


def _decode_text(raw, cap):
    """Decode the device's raw TEXT output into cap. Each timestamp line is '<ch> <sec>.<ns> <+|->'
    with exactly nine ns digits. The stream is pure ASCII and line-framed, so anything unparseable
    is corruption and raises -- with one structural exception: the capture window closes at an
    arbitrary instant, so a final partial line (no trailing newline) is dropped. The start needs no
    exception: captures begin marker-synced, on a line boundary."""
    try:
        text = raw.decode("ascii")
    except UnicodeDecodeError as e:
        raise RuntimeError(f"non-ASCII byte in text stream: {e}")
    lines = text.split("\n")
    lines.pop()  # tail after the last newline: empty, or a torn partial
    for line in lines:
        if line.startswith("#"):
            cap.comments.append(line)
            m = _DIAG_RE.search(line)
            if m:
                cap._add_loss(int(m.group(1)), int(m.group(2)), int(m.group(3)))
            continue
        parts = line.split()
        try:
            if len(parts) != 3 or parts[2] not in ("+", "-"):
                raise ValueError
            ch = int(parts[0])
            sec_str, dot, ns_str = parts[1].partition(".")
            if not (0 <= ch <= 3) or dot != "." or len(ns_str) != 9:
                raise ValueError
            t_ns = int(sec_str) * NS + int(ns_str)
        except ValueError:
            raise RuntimeError(f"corrupt text line: {line!r}")
        cap._add(ch, t_ns, parts[2])


def decode_text(raw):
    """Decode raw TEXT-wire bytes into a Capture (see _decode_text for the framing rules).
    For incremental consumers that accumulate bytes and re-decode; capture() is the normal path."""
    cap = Capture("text")
    _decode_text(raw, cap)
    return cap


def _ingest(cap, records):
    for r in records:
        if isinstance(r, Timestamp):
            cap._add(r.channel, r.seconds * NS + r.nanoseconds, r.polarity)
        elif isinstance(r, PulsesLost):
            cap._add_loss(r.channel, r.overcaptures, r.buf_overflows)
        elif isinstance(r, OscillatorFailure):
            raise RuntimeError(
                "reference clock failed -- device halted; restore the 10 MHz source and reset"
            )


def capture(tic, wire, duration_s, settle_s=0.0, discard=True):
    """THE capture path: configure the wire format, discard everything prior (plus settle_s of new
    data when the source was just reconfigured), then collect for duration_s and return a Capture.
    A reference-clock failure raises.

    discard=False skips the format command and marker sync and decodes the live stream exactly as
    it arrives. Only valid where the caller knows the stream is record-aligned and in the expected
    format -- e.g. resuming after OUTPut:STATe OFF, where the TX path has only ever emitted whole
    records. This is how a test can observe the silenced-period backlog that a marker sync would
    destroy."""
    cap = Capture(wire)
    if wire == "text":
        if discard:
            tic.send("FORM:DATA TEXT")
            tic.set_stream_enabled(True)
            tic.discard_pending(settle_s=settle_s)
        _decode_text(tic.read_raw(duration_s), cap)
        return cap

    if discard:
        # read_for does the FORM switch + marker-synced framing; its reader thread drains the port
        # independently of decode speed, so even full-rate captures can't backpressure the device.
        tic.discard_pending(settle_s=settle_s)
        _ingest(cap, tic.read_for(duration_s))
    else:
        _ingest(cap, tsctl._decode_chunks([tic.read_raw(duration_s)]))
    return cap


def arrivals(tic, wire, channel, window_s, want):
    """Host monotonic arrival time of each record on `channel`, for up to window_s, stopping once
    `want` are seen. Unlike capture(), this decodes live by necessity -- the arrival timing is the
    measurand -- so it is only suitable for low-rate phases."""
    if wire == "text":
        tic.send("FORM:DATA TEXT")
        tic.set_stream_enabled(True)
        tic.discard_pending()
        out, raw, seen = [], b"", 0
        start = time.monotonic()
        while time.monotonic() - start < window_s and len(out) < want:
            chunk = tic.read_raw(0.05)
            if not chunk:
                continue
            raw += chunk
            n = len(decode_text(raw).records(channel))
            while seen < n:
                out.append(time.monotonic())
                seen += 1
        return out

    out = []
    deadline = time.monotonic() + window_s
    for r in tic.read_for(window_s):
        if isinstance(r, Timestamp) and r.channel == channel:
            out.append(time.monotonic())
            if len(out) >= want:
                break
        if time.monotonic() >= deadline:
            break
    return out


# --------------------------------------------------------------------
# The check collector
# --------------------------------------------------------------------


class Phase:
    """Collects a phase's expectations. Unlike a hand-threaded `ok &= expect(...)` chain, a check
    registered here can never be silently dropped."""

    def __init__(self):
        self.ok = True

    def expect(self, condition, message):
        print(f"  [{'PASS' if condition else 'FAIL'}] {message}")
        self.ok = self.ok and bool(condition)
        return bool(condition)


# --------------------------------------------------------------------
# The scoring vocabulary
# --------------------------------------------------------------------


def count_tol(rate_hz, base):
    """Expected-count tolerance for a capture window. The window opens and closes with tens of ms of
    host-side jitter, clipping ~rate*jitter records at each end, so the budget scales with the
    record rate; `base` preserves low-rate slack."""
    return max(base, int(rate_hz * 0.1))


def report_collected(cap, ch, expected):
    times = cap.times(ch)
    others = sorted(cap.channels() - {ch})
    print(
        f"  collected {len(times)} on ch{ch} "
        f"(expected ~{expected:.1f}); others={others or 'none'}"
    )


def report_intervals(times):
    if len(times) < 2:
        print("  intervals: <2 samples")
        return
    deltas = [b - a for a, b in zip(times, times[1:])]
    print(
        f"  intervals: count={len(deltas)} "
        f"min={min(deltas) / 1e6:.3f} ms "
        f"median={statistics.median(deltas) / 1e6:.3f} ms "
        f"max={max(deltas) / 1e6:.3f} ms"
    )


def expect_gaps(ph, deltas, target_ns, label, tol_ns=GAP_TOL_NS):
    """Every gap in `deltas` equals target_ns within tol_ns. On failure, the violators are printed
    with deviations in ns so a boundary artifact vs a real timing fault is immediately
    distinguishable (a clean gap is a few ns off; a missed pulse is a whole period off)."""
    bad = [(i, d) for i, d in enumerate(deltas) if abs(d - target_ns) >= tol_ns]
    if bad:
        shown = ", ".join(f"#{i}={d / 1e6:.4f}ms" f"(Δ{d - target_ns:+d}ns)" for i, d in bad[:6])
        print(
            f"    [DIAG] {label}: {len(bad)}/{len(deltas)} gaps off "
            f"{target_ns / 1e6:.3f} ms by >={tol_ns} ns: {shown}" + (" …" if len(bad) > 6 else "")
        )
    return ph.expect(
        not bad,
        f"{label}: every gap within {tol_ns} ns of " f"{target_ns / 1e6:.3f} ms (no pulse missed)",
    )


def expect_cadence(ph, times, period_ns, label, tol_ns=GAP_TOL_NS, edges="all"):
    """Every gap between consecutive timestamps equals period_ns within tol_ns. edges="interior"
    excludes the first and last gap: those touch the capture-window boundary, where a record can be
    clipped, so a real miss there is indistinguishable from the window edge -- a true device miss is
    interior and deviates by a whole period, orders of magnitude past tol."""
    if len(times) < (4 if edges == "interior" else 2):
        return ph.expect(False, f"{label}: too few records " f"({len(times)}) to score gaps")
    deltas = [b - a for a, b in zip(times, times[1:])]
    if edges == "interior":
        deltas = deltas[1:-1]
    return expect_gaps(ph, deltas, period_ns, label, tol_ns)


def expect_rate(ph, times, hz, label, tol_ppm=50):
    """Average rate from the timestamps themselves -- count over the first-to-last span -- against
    the configured rate. Per-gap checks can hide a systematic rate error; this can't."""
    if len(times) < 2:
        return ph.expect(False, f"{label}: too few records to rate")
    obs_hz = (len(times) - 1) * NS / (times[-1] - times[0])
    err_ppm = (obs_hz / hz - 1.0) * 1e6
    return ph.expect(
        abs(err_ppm) <= tol_ppm,
        f"{label}: observed {obs_hz:.1f} Hz vs expected " f"{hz:g} Hz ({err_ppm:+.1f} ppm)",
    )


def expect_polarity(ph, pols, want):
    """Every record's hardware-latched polarity column is `want`."""
    bad = [i for i, p in enumerate(pols) if p != want]
    diag = f"; {len(bad)} wrong, first at #{bad[0]}" if bad else ""
    return ph.expect(not bad, f"all {len(pols)} records have polarity " f"'{want}'{diag}")


def expect_no_loss(ph, cap):
    """Both wires report loss in-band -- the '# ch<N>: ...' line in text, the PulsesLost record in
    binary -- so assert zero either way."""
    ph.expect(cap.overcaptures == 0, "no overcaptures")
    ph.expect(cap.buf_overflows == 0, "no ring-buffer overflows")


def expect_quiet_others(ph, cap, active):
    """No records may leak in from channels outside the active set."""
    others = sorted(cap.channels() - set(active))
    ph.expect(not others, f"no spurious other-channel activity ({others or 'none'})")


def expect_monotonic(ph, times):
    """Timestamps must be nondecreasing: a backwards step is a merge mis-order, never measurement
    noise."""
    bad = [i for i in range(len(times) - 1) if times[i + 1] < times[i]]
    if bad:
        i = bad[0]
        print(
            f"    [DIAG] first backwards step at #{i}: "
            f"{times[i]} -> {times[i + 1]} "
            f"({times[i + 1] - times[i]:+d} ns)"
        )
    ph.expect(not bad, f"timestamps in order ({len(bad)} backwards steps)")


def expect_substreams_monotonic(ph, records):
    """Each polarity's records must arrive in time order (call on raw, unsorted capture records): a
    backwards step within one sub-stream is a device bug, never a window or batching artifact."""
    for want in ("+", "-"):
        sub = [t for t, p in records if p == want]
        bad = sum(1 for i in range(len(sub) - 1) if sub[i + 1] < sub[i])
        ph.expect(
            bad == 0, f"'{want}' sub-stream arrives in time order " f"({bad} backwards steps)"
        )


def trim_window(records, guard_ns=250_000_000):
    """Drop records within guard_ns of the capture edges (call on time-sorted records). The two sub-
    streams arrive in flush batches up to ~100 ms apart, so cutting the capture window mid-flush can
    clip one polarity's final batch and leave the other artificially unpaired at the tail; per-edge
    assertions only run on the interior."""
    if not records:
        return records
    lo = records[0][0] + guard_ns
    hi = records[-1][0] - guard_ns
    return [(t, p) for t, p in records if lo <= t <= hi]


def classify_gaps(deltas, split_ns):
    """Split gaps into short (< split_ns) / long (>= split_ns). The two populations sit far apart
    (e.g. 5 us vs 495 us), so the split is unambiguous. Returns (classes, shorts, longs)."""
    cls = ["S" if d < split_ns else "L" for d in deltas]
    short = [d for d, c in zip(deltas, cls) if c == "S"]
    long = [d for d, c in zip(deltas, cls) if c == "L"]
    return cls, short, long


def split_bursts(times, span_ns=500_000_000):
    """Group times into bursts: every record within span_ns of a burst's first record belongs to
    that burst (the PG-4 repeats bursts at 1/s, so 0.5 s is unambiguous). Returns a list of bursts,
    each a list of times."""
    bursts, i = [], 0
    while i < len(times):
        j = i + 1
        while j < len(times) and times[j] - times[i] < span_ns:
            j += 1
        bursts.append(times[i:j])
        i = j
    return bursts


def firings(cap, split_ns, channels=CHANNELS):
    """Group every channel's records into firings (a gap > split_ns starts a new firing). Returns
    (complete, interior_incomplete): `complete` is [{channel: t_ns}] for firings with exactly one
    record on each channel; `interior_incomplete` counts firings missing a channel even though they
    lie strictly inside every channel's coverage -- a missed edge, which callers must treat as a
    failure. Firings at the coverage edges may legitimately be partial (the window, or a channel's
    final emission batch, clips them) and are excluded from both."""
    if not all(cap.records(c) for c in channels):
        return [], 0
    tagged = sorted((t, c) for c in channels for t in cap.times(c))
    groups, cur, last = [], {}, None
    for t, c in tagged:
        if last is not None and t - last > split_ns:
            groups.append(cur)
            cur = {}
        cur.setdefault(c, []).append(t)
        last = t
    if cur:
        groups.append(cur)
    lo = max(min(cap.times(c)) for c in channels)
    hi = min(max(cap.times(c)) for c in channels)
    complete, interior_bad = [], 0
    for f in groups:
        if all(len(f.get(c, [])) == 1 for c in channels):
            complete.append({c: f[c][0] for c in channels})
        else:
            ts = [t for c in channels for t in f.get(c, [])]
            if ts and min(ts) > lo and max(ts) < hi:
                interior_bad += 1
    return complete, interior_bad


def median_offsets(complete, channels=CHANNELS):
    """Median offset (ns) of each channel from ch0 across complete firings."""
    return {c: statistics.median(f[c] - f[0] for f in complete) for c in channels}


def expect_firing_spread(ph, complete, med, tol_ns=GAP_TOL_NS, channels=CHANNELS):
    """Every individual firing's per-channel offsets must sit within tol_ns of the medians -- a
    single anomalous firing is a failure, never averaged away."""
    spread = max(abs((f[c] - f[0]) - med[c]) for f in complete for c in channels)
    return ph.expect(
        spread <= tol_ns,
        f"every firing's offsets within {tol_ns} ns of the median (worst {spread:.1f} ns)",
    )


# --------------------------------------------------------------------
# Fixtures
# --------------------------------------------------------------------


def configure(tic, slopes=None, dividers=None):
    """Device-side channel config, fully specified: every channel gets a slope and divider (defaults
    POS / 1), so no test inherits a prior test's settings."""
    slopes = slopes or {}
    dividers = dividers or {}
    for c in CHANNELS:
        tic.set_slope(c, slopes.get(c, "POS"))
        tic.set_divider(c, dividers.get(c, 1))


class Source:
    """Known-state PG-4 driver. Every method fully specifies the generator state it touches -- mode,
    burst state, per-channel period/width/delay/output -- and verifies the PG-4 accepted it, so no
    test inherits leftovers (a stale burst config, a mode, an output left running) from whatever ran
    before."""

    def __init__(self, pg):
        self.pg = pg

    def _check(self, what):
        err = self.pg.get_error()
        if not err.startswith("0,"):
            raise RuntimeError(f"PG-4 rejected {what}: {err}")

    def off(self):
        self.pg.off()

    def _async_base(self):
        # Outputs off FIRST: clearing burst state while an output is live momentarily runs it
        # CONTINUOUSLY at the old intra-burst spacing (a ~20 MHz blast after a minimum-spacing
        # burst phase), which sets overcapture flags on the device under test.
        for c in CHANNELS:
            self.pg.set_state(c, False)
        self.pg.set_mode(Pulsegen.ASYNC)
        for domain in (0, 2):  # one burst controller per period domain
            self.pg.set_burst_state(domain, False)

    def periodic(self, channel, hz, width_s):
        """One channel running continuously; everything else off."""
        self._async_base()
        self.pg.set_period(channel, 1.0 / hz)
        self.pg.set_width(channel, width_s)
        self.pg.set_delay(channel, 0)
        self.pg.set_state(channel, True)
        self._check(f"periodic ch{channel} {hz} Hz")

    def async_rates(self, hz01, hz23, duty=0.25):
        """All four channels on: ch0/1 at hz01, ch2/3 at hz23 (the PG-4's two independent ASYNC
        period domains)."""
        self._async_base()
        for c, hz in ((0, hz01), (1, hz01), (2, hz23), (3, hz23)):
            self.pg.set_period(c, 1.0 / hz)
            self.pg.set_width(c, duty / hz)
            self.pg.set_delay(c, 0)
            self.pg.set_state(c, True)
        self._check(f"async rates {hz01}/{hz23} Hz")

    def sync(self, delays_ns, period_ns, width_ns=1000):
        """All four channels phase-locked at one period with the given per-channel rising-edge
        delays (ns)."""
        for c in CHANNELS:  # outputs off first; see _async_base
            self.pg.set_state(c, False)
        self.pg.set_mode(Pulsegen.SYNC)
        self.pg.set_burst_state(0, False)  # sync = one domain
        for c in CHANNELS:
            self.pg.set_period(c, period_ns / NS)
            self.pg.set_width(c, width_ns / NS)
            self.pg.set_delay(c, delays_ns[c] / NS)
            self.pg.set_state(c, True)
        self._check(f"sync period {period_ns} ns")

    def burst(self, channel, spacing_ns, ncyc):
        """Repeating hardware-exact burst on one channel (1/s rep); everything else off. Returns the
        quantized actual spacing (ns)."""
        self._async_base()
        actual_ns = self.pg.pulse_burst(channel, spacing_ns, ncyc)
        self._check(f"burst ch{channel} {ncyc}x{spacing_ns} ns")
        return actual_ns
