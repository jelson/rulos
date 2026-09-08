# PG-4 / LectroTIC-4 Regression Testbed

The paired suites use four straight-through connections: PG-4 output N to
LectroTIC-4 input N, for channels 0 through 3. Both boards need USB and the
same 10 MHz reference. The optional BMP auxiliary UART drives the LT4 serial
input; a BMP on the LT4 SWD header enables persistence/reset phases.

Use explicit stable `/dev/serial/by-id/` paths when multiple instruments or
probes are attached. Check each instrument's identity and board revision
before running or flashing. The LT4 build defaults to Rev C; the Rev B LT4
needs its own pin configuration.

## Existing Suites

Run these commands from the repository root with Python 3 and pyserial.
The examples assume `LT4` and `PG4` contain the two native USB device paths.

| Entry point | Coverage |
| --- | --- |
| `src/app/pulsegen/test/regression_test.py` | Independent channels, HRTIM/GP handoff, slow periods, sync phase, bursts, pin routing, and sub-tick placement. Select a group with `--only`. |
| `src/app/timestamper/test/regression_test.py` | Capture slope/division, output gating, finite bursts, overload reporting, sustained rates, UART timestamps, reset/configuration, CLI behavior, and four-channel timing. Select with `--phase`, `--skip`, and `--channel`. Most capture phases run on both text and binary wires. |
| `src/app/timestamper/test/util_test.py` | Real subprocess tests of frequency, phase, pulse-width, and Allan-deviation utilities, plus hardware-free reader/math checks. |
| `src/app/timestamper/test/deadtime.py` | Search the minimum spacing for finite captured bursts. |
| `src/app/timestamper/test/sustained_rate.py` | Search sustained USB throughput, separately for text and binary. |
| `src/app/timestamper/test/control_test.py` | USB reply backpressure, reconnect behavior, optional UART contention, and opt-in flash-error/persistence testing. Does not need a PG-4. |

```sh
python3 src/app/pulsegen/test/regression_test.py \
  --ts-port "$LT4" --pg-port "$PG4"
python3 src/app/timestamper/test/regression_test.py \
  --port "$LT4" --pg-port "$PG4"
```

The utility suite's `--pg-port` selects the generator; its LT4 subprocesses
autodetect the timestamper, so run it with only one LT4 connected.

## State And Safety

These are active bench tests: they change output signals and acquisition
settings. Preserve firmware and saved configuration before a run if they
must be restored exactly afterward.

- The PG suite calls LT4 `*RST` during setup and cleanup. On this firmware,
  `*RST` also saves default channel settings; it is not just a volatile reset.
- LT4 `config persist` and `config torn` phases write saved settings and reset
  the board. `--skip config` excludes those phases, but is not a general
  guarantee that no other selected test writes configuration.
- The control suite's `--persistence` explicitly enables its flash-failure
  and persistence cases; its ordinary USB tests do not save or flash.
- `factory.sh` builds, flashes, verifies, and resets a manufacturing unit.
  Do not use it as a wrapper for regression testing an existing Rev B board.
- Only one process may own a device's USB/UART connection at a time. Do not
  run these suites concurrently on the same pair.

## Measurement Rules

LT4 phases use `tstest.Source`, `tstest.capture`, `tstest.Capture`, and
`tstest.Phase` rather than separate transport or decoding implementations.
PG measurements use the existing PG regression helpers and `tsctl` reader.
Keep timestamps as integer nanoseconds and retain the shared 12 ns timing
tolerance unless a test explicitly accounts for a coarser GP timer tick.

Arrival order is not global time order: the LT4 sends rising/falling DMA
sub-streams in separate batches. Sort when comparing chronological edges,
but check exact selected edges, polarity, cadence, and loss diagnostics;
correct aggregate counts alone cannot prove a divider is correct.

For pulse widths, the PG scorer restricts steady-state measurements to the
common coverage of the rising and falling streams. Capture boundaries and
independent DMA batches can leave unmatched edges outside that coverage;
the discarded span may not exceed one nominal period plus 250 ms of batch
skew. Interior missing, duplicate, or nonalternating edges still fail.
Rising-edge cadence uses the full capture. Armed startup captures never
trim the first edge, first pulse width, or first gap; only incomplete trailing
width coverage may be excluded.

Normal capture setup sends `OUTP:CLE` to establish framing. Startup tests
must establish framing before enabling the source and must not clear or
discard afterward. Ring-backlog tests must select the wire and clear before
filling the ring, then capture with `discard=False`. First-edge and
first-period regressions must not trim away the first samples or bursts.
USB command completion is not a timestamped enable marker: startup checks
score the first observed waveform, not command-to-first-edge latency. Wide
startup pulses keep the known stale-prescaler waveform within LT4 capture
capability; there is no claim that an entirely absent first pulse can always
be distinguished from the capture boundary.

New failure cases assert the intended behavior, not the current faulty
output. A hardware failure is a reproduction to investigate, not an expected
failure to suppress. Host-only synthetic tests validate the scorers and
sequencing; they do not establish that either physical board passes.

## Targeted Bug Regressions

Run these before changing either firmware so the same measurements can be
repeated after each fix. The GP groups exercise all four pulse connections;
the TIM5 repetition group uses channel 0. LT4 phases can first be isolated
to one channel. All measurements require the common reference.

```sh
python3 src/app/pulsegen/test/regression_test.py \
  --ts-port "$LT4" --pg-port "$PG4" --only gp-startup
python3 src/app/pulsegen/test/regression_test.py \
  --ts-port "$LT4" --pg-port "$PG4" --only gp-enable
python3 src/app/pulsegen/test/regression_test.py \
  --ts-port "$LT4" --pg-port "$PG4" --only gp-wrap
python3 src/app/pulsegen/test/regression_test.py \
  --ts-port "$LT4" --pg-port "$PG4" --only rep-startup
python3 src/app/timestamper/test/regression_test.py \
  --port "$LT4" --pg-port "$PG4" --channel 0 --phase 'divider both'
python3 src/app/timestamper/test/regression_test.py \
  --port "$LT4" --pg-port "$PG4" --channel 0 --phase 'divider ring pressure'
```

- `gp-startup` primes the old prescaler with observed periods, then captures
  the first pulse and gap after switching between 1 ms and 1 s periods.
  It exercises each GP output timer without assuming power-on register state.
- `gp-enable` keeps the prescaler unchanged: each of the four GP timers is
  primed at 1 s, then enabled again at 1 s with a 750 ms pulse width. The
  first observed edge, width, and gap are scored without startup trimming.
  This isolates an output-initialization glitch from stale PSC startup
  behavior. The baseline hardware run reproduced a short extra leading
  pulse on all four channels with PSC unchanged; both defects may
  ultimately share a timer-initialization fix.
- `gp-wrap` sweeps delays near a 1 ms period boundary with 10 us-wide pulses,
  in ASYNC and SYNC. Every complete pulse must have the requested width;
  missing or repeated polarities are failures, not pairs to skip silently.
- `rep-startup` changes TIM5's repetition prescaler in both directions.
  Its long case includes a 41-second capture: a 40-second interval requires
  a different prescaler from the ordinary short burst tests. The first frame
  and first repetition are part of the measurement.
- `divider both` checks divisors 2, 3, and 5 against chronological edge
  selection. Even divisors must retain one polarity; odd divisors must
  alternate polarity with the corresponding short/long selected-edge gaps.
- `divider ring pressure` preloads 16,000 records with output gated off,
  then adds 1,148 pulses at divider 3. The 382 new records fit without loss.
  After draining, one more pulse must emit a record because the divider has
  two edges of progress left over. No marker clear occurs between stages.

Hardware-free scorer and sequencing checks:

```sh
python3 src/app/pulsegen/test/regression_test_test.py
python3 src/app/timestamper/test/regression_test_test.py
python3 src/app/timestamper/test/control_test_test.py
python3 src/app/timestamper/test/util_test.py --only reader_error
python3 src/app/timestamper/test/util_test.py --only allan_math
```

The first two commands exercise accepted and deliberately faulty traces,
including the previously observed failure patterns. Actual reproductions
on the paired boards remain necessary; synthetic passes are not bench
results.
