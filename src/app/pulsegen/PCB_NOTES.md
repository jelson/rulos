# Pulsegen PCB notes

Hardware target: **STM32G474RE** (LQFP64, `stm32g474xe` in the build
system: 512 KB flash, 96 KB SRAM). LQFP64 is needed for Port C access.

---

# Rev B (firmware written 2026-06-14; not yet fabricated)

**Goal:** 4 independently-programmable periods, AND a SYNC mode where all
four phase-lock to one shared period, spanning ~24 ns to ~34 s (HRTIM
hands off to a GP timer above HRTIM's ~524 µs ceiling). Rev A (PC6-9)
couldn't: its outputs reach only HRTIM Timers E/F (2 timers → 2 shared
periods) and only TIM3 as a GP fallback (one shared ARR).

The pinout below was fully verified against the datasheet (DS12288 Rev 6)
and reference manual (RM0440 Rev 9) **before any layout** — the checks in
the next section are all marked CONFIRMED — and the firmware is written
and builds against it.

## Output timers (the load-bearing choice)

Channel→pin assignment is the laid-out Rev B routing (2026-06-14); the
per-pin hardware is fixed by the silicon.

| Ch | Pin  | HRTIM (fast)   | HRTIM AF | GP timer (slow)        | GP AF |
|----|------|----------------|----------|------------------------|-------|
| 0  | PC8  | Timer E, CHE1  | AF3      | TIM8_CH3               | AF4   |
| 1  | PC6  | Timer F, CHF1  | AF13     | TIM3_CH1               | AF2   |
| 2  | PA9  | Timer A, CHA2  | AF13     | TIM2_CH3 (TIM2 32-bit) | AF10  |
| 3  | PA10 | Timer B, CHB1  | AF13     | TIM1_CH3               | AF6   |

All AF numbers are **verified against DS12288 Rev 6 Table 13** by column
position (2026-06-14). Four distinct HRTIM timers (A/B/E/F), four distinct
GP timers (TIM2/TIM1/TIM3/TIM8), TIM2 is 32-bit. (The firmware maps each
channel to its pin via a per-channel table, so this assignment can be
re-routed without touching logic -- the GP-sync master, found as whichever
channel owns TIM1, and the GP-burst ISRs, keyed by timer, both follow.) The HRTIM AF is per-pin
(PC8/Timer E is AF3, the other three AF13) and the GP AF differs on every
pin (AF10/AF6/AF2/AF4) — so the firmware MUST keep AF as a per-channel
field, never a shared constant. (A web snippet during verification showed
only PA9=TIM1_CH2 and nearly triggered a false "PA9 can't reach TIM2"
alarm — the datasheet shows PA9 carries BOTH TIM1_CH2/AF6 and
TIM2_CH3/AF10. Trust the datasheet, not the snippet.)

## Complete pin map

Every pin the firmware and board depend on (the Rev B analog of Rev A's
"Critical pin choices"). Outputs, USB, SWD, HSE, and the debug UART are
load-bearing; LEDs are flexible (change in `pulsegen.c` if rerouted).

| Net          | MCU pin | Why |
|--------------|---------|-----|
| PULSE_OUT_0  | **PC8**  | HRTIM1_CHE1 **AF3** (NOT AF13 — Timer E differs!) + TIM8_CH3 **AF4** (advanced → BDTR.MOE). |
| PULSE_OUT_1  | **PC6**  | HRTIM1_CHF1 **AF13** + TIM3_CH1 **AF2**. |
| PULSE_OUT_2  | **PA9**  | HRTIM1_CHA2 **AF13** + TIM2_CH3 **AF10** (slow, 32-bit). |
| PULSE_OUT_3  | **PA10** | HRTIM1_CHB1 **AF13** + TIM1_CH3 **AF6** (advanced → MOE). |
| OSC_IN       | PF0     | 10 MHz HSE input (bypass; PF1/OSC_OUT free for GPIO). |
| USB_DM       | PA11    | Fixed by silicon. |
| USB_DP       | PA12    | Fixed by silicon. |
| UART_TX      | **PB6** | USART1_TX, AF7 (rulos `uart_id=0`). Off the output pins. |
| UART_RX      | **PB7** | USART1_RX, AF7. |
| SWDIO        | PA13    | Fixed by silicon. |
| SWCLK        | PA14    | Fixed by silicon. |
| LED ch0      | PC0     | (flexible) |
| LED ch1      | PA3     | (flexible) |
| LED ch2      | PA4     | (flexible) |
| LED ch3      | PA8     | (flexible) |
| LED HSE      | PB4     | (flexible) HSE health heartbeat. |
| LED USB      | PA5     | (flexible) USB enumerated/activity. |

**Internal-only (no pin to route):** the slow-regime Combined-PWM output
uses each channel's *sibling* timer channel for the second edge, with its
output stage **disabled** (CCyE=0), so it consumes a CCR register but
drives no pin: ch0 TIM8_CH4, ch1 TIM3_CH2, ch2 TIM2_CH4, ch3 TIM1_CH4.
Two of those siblings happen to map to pins used by other nets
(TIM2_CH4 → PA10 = PULSE_OUT_3; TIM1_CH4 → PA11 = USB_DP) — harmless,
because the sibling's output is never enabled, so its timer doesn't drive
the pin. PC7 and PC9 (the old Rev A outputs) are free.

## The checks

1. **Each pin is on a *distinct* HRTIM timer.** Confirm the four outputs
   land on four *different* HRTIM timers (A/B/E/F above). Two outputs on
   the same HRTIM timer share one counter/period — Rev A's
   defect that caps it at 2 independent periods. [DS12288 pin/AF table.]

2. **Each pin *also* reaches a *distinct* GP timer.** Confirm every pin
   has a GP/advanced-timer channel too, on four *different* timers
   (TIM1/2/3/8), so the long periods are independent. Confirm one is
   32-bit (TIM2) for the longest periods. [DS12288 pin/AF table.]

3. **Those four GP timers can be mutually synchronized.** — **CONFIRMED
   (RM0440 Rev 9, 2026-06-14).** TIM1 is master (MMS=update → tim1_trgo);
   TIM2/TIM3/TIM8 each slave-reset on **ITR0 = tim1_trgo**: Table 291
   gives TIM2 ITR0 = tim1_trgo and TIM3 ITR0 = tim1_trgo; Table 267 (the
   advanced-timer-as-slave table) gives TIM8 ITR0 = tim1_trgo. So in
   GP-timer SYNC, set TIM1 TRGO=update, the other three SMS=reset on ITR0,
   preset all counters to 0 before a coordinated enable (the validated
   recipe below). TIM1 doubles as ch1's output timer and the sync master —
   no conflict (it emits TRGO and drives CH3).

4. **Each pin's alternate-function number — looked up INDIVIDUALLY.** This
   is the bug that cost us a board generation. HRTIM outputs are NOT all
   on the same AF: on this chip PC6 CHF1 = **AF13** but PC8 CHE1 = **AF3**.
   The firmware hardcoded AF13 for all four, so the Timer-E channels
   emitted nothing — undetected until a 4-channel test finally drove them.
   Verify EACH pin's HRTIM AF and EACH pin's GP-timer AF as separate
   lookups (and keep them in a per-channel firmware table, not a shared
   constant). Status — **all CONFIRMED against DS12288 Rev 6 Table 13
   (2026-06-14):**
   - HRTIM: PA9 CHA2 = AF13, PA10 CHB1 = AF13, PC6 CHF1 = AF13,
     PC8 CHE1 = **AF3** (Timer E is the odd one out, as on the old board).
   - GP timer: PA9 TIM2_CH3 = AF10, PA10 TIM1_CH3 = AF6,
     PC6 TIM3_CH1 = AF2, PC8 TIM8_CH3 = AF4.

5. **No collision with the fixed-function pins.** — **RESOLVED.** PA9/PA10
   become outputs, so the debug UART moves off them. Decision: keep it on
   **USART1 but on PB6 (TX) / PB7 (RX)** — both USART1 AF7, both free —
   which needs **zero rulos-driver change** (just two -D macros:
   RULOS_UART0_TX_PIN=GPIO_B6, RULOS_UART0_RX_PIN=GPIO_B7). (USART2 on
   PA2/PA3 would have worked too but needs a driver config entry; PB6/PB7
   is simpler.) Confirmed clear: PA11/12 = USB, PA13/14 = SWD, PF0 = HSE.
   LEDs unchanged from the old board (none of PC0/PA3/PA4/PA8/PB4/PA5
   collide with the new outputs or PB6/PB7).

6. **Package exposes every pin.** LQFP64 (Port C is needed for PC6/PC8;
   LQFP48 has none). Confirm the chosen variant brings out all of the
   above.

## Rev B firmware architecture (finalized 2026-06-14)

**Design rule (jelson): full feature parity across the HRTIM and GP-timer
regimes — no arbitrary ~524 µs cliff where behavior changes. If a feature
can't be supported in both regimes, it isn't implemented in either.** The
handoff is invisible: you set a period anywhere in 24 ns–34 s and the
firmware picks the timer. The ONLY thing that changes at the crossover is
edge-placement granularity (250 ps below ~524 µs, 8 ns above) — a
precision detail, not a behavior change, and 8 ns is ~15 ppm at 524 µs
(imperceptible at those rates).

- **Per-channel regime / handoff:** period ≤ HRTIM ceiling (≈524 µs,
  CKPSC=5 × 0xFFDF) → HRTIM (250 ps); above → the channel's own GP timer.
  The pin's AF is switched HRTIM-AF ↔ GP-AF at the crossover. Each channel
  is now ALONE on its HRTIM timer, so HRTIM uses CMP1 (rise) / CMP2 (fall)
  for every channel — no Rev A "slot" sharing.
- **ASYNC = 4 independent periods** (each channel its own domain/timer);
  **SYNC = all four share one period+phase.** Delay is a SYNC concept
  (async channels free-run with no shared t=0, so delay is unobservable
  there — a mode property, uniform across the whole range, not a cliff).
- **GP-timer output = Combined PWM mode (verified RM0440 §29.3.15 /
  §30.4.13).** A single GP channel has one CCR = one edge; Combined PWM
  gangs the channel's CCR with its sibling's onto one output to place both
  edges. Recipe for a [D, D+W] pulse on the routed channel:
  - output channel: **Combined PWM mode 2 (OCxM=1101)**, CCR = D (its ref
    is PWM-mode-2 = high D..ARR); the combined ref OCxREFC = OCxREF **AND**
    OCyREF.
  - sibling channel: **PWM mode 1 (OCyM=0110)**, CCR = D+W (high 0..D+W),
    output NOT enabled (CCyE=0).
  - AND ⇒ high only in [D, D+W]. Works for D=0, so it's the single uniform
    GP output path. Siblings: ch0 TIM8_CH4, ch1 TIM3_CH2, ch2 TIM2_CH4,
    ch3 TIM1_CH4 (output off → PA11 stays USB).
    Advanced timers (TIM1/TIM8) need BDTR.MOE=1.
- **SYNC master follows the regime:** ≤524 µs → HRTIM master resets slaves
  A/B/E/F on MASTER_PER; above → TIM1 master (TRGO=update), TIM2/TIM3/TIM8
  slave-reset on ITR0; all counters preset to 0 before a single coordinated
  enable (the validated recipe below).
- **Burst = uniform behavior, two implementations:** HRTIM BMC when fast
  (software can't count MHz pulses); a software per-period count (GP update
  IRQ, <2 kHz there) when slow. Same "exactly N then rest, repeat" either
  way; no RCR needed (TIM2/TIM3 lack it — irrelevant).
- **HRTIM cannot reach long periods (verified RM0440):** no inter-timer
  clocking (every HRTIM counter is fHRTIM/prescaler only — no cascade), the
  8-bit repetition counter only sets the interrupt rate (not the output),
  and the single BMC reaches ~34 s but quantized to the base period and
  not replicable per channel. So the GP-timer fallback is necessary, not a
  workaround.
- **HRTIM<->GP HANDOFF: no GP-timer fallback existed on the old board.**

## First-silicon bring-up checklist (verified in code; confirm on hardware)

The firmware compiles and was design-reviewed, but these compile-clean and can
only be confirmed on a real board:

- **Combined-PWM sibling mode.** The slow-regime output uses Combined PWM mode 2
  on the routed channel + plain PWM mode 1 on the sibling, per DS/RM Figure 337.
  The RM prose alternatively says the sibling should be in the *opposite combined*
  mode; the figure (plain PWM1) is what's coded. Scope the GP-regime output first
  thing: confirm width and delay land where asked at, say, a 1 ms period.
- **GP-regime SYNC alignment.** Slaves (TIM2/TIM3/TIM8) are preset to 0 and
  enabled, then TIM1 (master) is enabled; the master's update (TRGO) resets the
  slaves each period via ITR0. Expect a few-instruction (~tens of ns) startup skew
  before the first master reset; confirm the four edges coincide within tolerance
  once running (this is the analog of the Rev A cross-timer sync work).
- **GP-regime burst exactness.** Software-counted on the domain's timebase
  (TIM1 in sync, the channel in async), gating outputs on period boundaries.
  Confirm exactly ncyc pulses per frame at a slow period (e.g. 1 ms x 5).
- **HRTIM AF per pin** (PA9/PA10/PC6 = AF13, PC8 = AF3) and **GP AF per pin**
  (AF10/AF6/AF2/AF4): drive every channel in both regimes and confirm all four
  actually emit (the Rev A Timer-E-silent bug was exactly a wrong AF).
- **Burst repetition timer (TIM5).** The interval between burst frames is timed
  by TIM5 in hardware, not the scheduler, so it should be jitter-free and
  phase-coherent with the pulses. Confirm the burst-to-burst start interval
  holds to its setpoint two-sided (scope, or the regression test's rep check),
  and that a repetition as short as one frame + 1 ms still emits clean frames.

## Design-review findings already fixed in firmware (2026-06-14)

A self-review of the first firmware draft caught and fixed: (1) GP-regime SYNC
silently broke when ch1 was off, because TIM1 doubles as ch1's output AND the
sync master and was only set up when ch1 was on -- now the master timebase runs
independently; (2) GP burst in SYNC gated only one channel -- now it gates the
whole domain off the master count; (3) GP burst wasn't pulse-exact -- now it
opens/closes the run window on period boundaries in the update ISR. Plus parity
polish: all channels capped uniformly at ~34 s (TIM2's 32-bit reach deliberately
not exposed) and an O(1) prescaler calc.

## Burst on a hardware timer + idle/margin fix (2026-06-28)

The burst repetition interval moved off the RULOS scheduler onto a dedicated
hardware timer (TIM5, 32-bit, no pins): its update ISR re-arms each frame, so
the cadence is locked to the 125 MHz clock -- phase-coherent and jitter-free,
and the scheduler is out of every time-critical path. Two coupled fixes rode
along: the HRTIM BMC idle window is now sized by absolute time (~200 us, the
BMPER-ISR grace period) rather than a flat 8192 periods that ballooned to
seconds of dead time at slow periods; and validate() now requires the
repetition to exceed the *real* frame duration -- (idle + ncyc) periods for
HRTIM, (ncyc + 1) for GP -- plus a 1 ms guard, closing a hole where a too-short
interval re-armed the controller mid-frame. Side effects: max ncyc rose
63488 -> 65534, and the minimum repetition interval dropped from ~frame + 50 ms
to ~frame + 1 ms. None of this has run on hardware yet -- see the TIM5 bring-up
item above.

## Firmware patterns these checks enable (reuse, not hardware checks)

- **Biggest lesson: write and bench-exercise the firmware for every output
  on every timer — independent *and* synchronous — before committing the
  layout.** Both Timer-E bugs (wrong AF, no cross-group sync) stayed hidden
  until a 4-channel test drove ch2/ch3. Doing this first surfaces the
  pairing constraint, the per-pin AF, and the cross-timer sync needs while
  they're a code edit, not a respin.
- **HRTIM SYNC (validated):** master in CONTINUOUS mode (the default
  single-shot fires the master period event — both the slave reset and the
  burst-controller clock — only once); slaves reset on MASTER_PER; and zero
  every counter (LL_HRTIM_CounterReset) right before the single
  CounterEnable, or they resume at stale, per-timer-different CNT and land
  at random relative phase.
- **GP-timer SYNC (long periods):** same shape via standard master/slave —
  TIM1 TRGO drives TIM2/TIM3/TIM8 slave-reset on ITR0, counters preset to 0
  before a coordinated enable. Identical to the timestamper's TIM2↔TIM5
  phase-lock (measured 0 ns).
- **Seamless handoff:** per requested period, route each pin's AF to HRTIM
  (≤524 µs, 250 ps) or its GP timer (>524 µs, ~8 ns; ÷65536 prescaler on a
  16-bit timer reaches ~34 s), and in SYNC mode pick the matching master
  (HRTIM master vs TIM1).

---

# Rev A (as built, STM32G474RE / PC6-9 — RETIRED; tag `pulsegen-revA`)

Superseded by Rev B above. Kept for reference; the last firmware that ran
on this board is at git tag `pulsegen-revA`. Outputs reach only HRTIM
Timers E/F (two shared periods) — the limitation Rev B fixes.

## Critical pin choices

These were load-bearing for the Rev A firmware as written.

| Net          | MCU pin | Why                                                          |
|--------------|---------|--------------------------------------------------------------|
| PULSE_OUT_0  | **PC6** | HRTIM1_CHF1, **AF13**, and TIM3_CH1 (AF2). |
| PULSE_OUT_1  | **PC7** | HRTIM1_CHF2, **AF13**, and TIM3_CH2 (AF2). |
| PULSE_OUT_2  | **PC8** | HRTIM1_CHE1, **AF3** (NOT AF13 — Timer E differs from Timer F!), and TIM3_CH3 (AF2). |
| PULSE_OUT_3  | **PC9** | HRTIM1_CHE2, **AF3**, and TIM3_CH4 (AF2). |
| OSC_IN       | PF0     | HSE input, fed by the 10 MHz active oscillator. HSE bypass mode leaves PF1 (OSC_OUT) unused. |
| USB_DM       | PA11    | Fixed by silicon.                                            |
| USB_DP       | PA12    | Fixed by silicon.                                            |
| UART_TX      | PA9     | USART1_TX, AF7 (rulos `uart_id=0`).                          |
| UART_RX      | PA10    | USART1_RX, AF7.                                              |

These four pins each map to an HRTIM output (Timers E/F — **PC6/PC7 on
AF13, PC8/PC9 on AF3**) and a TIM3 channel (AF2). The shipped firmware is
**HRTIM-only**: the TIM3 long-period fallback was designed-for but never
implemented (and TIM3's single shared ARR couldn't give four independent
long periods anyway — that's the Rev B goal). So Rev A's
usable range is the HRTIM range, ~24 ns to ~524 µs.

### LEDs (flexible — change in `pulsegen.c` if you reroute)

| Function       | Pin     | Note |
|----------------|---------|------|
| Channel 0 act. | PC0     |      |
| Channel 1 act. | PA3     |      |
| Channel 2 act. | PA4     |      |
| Channel 3 act. | PA8     |      |
| HSE heartbeat  | PB4     |      |
| USB enumerated | PA5     |      |

## Headers

- **4× SMA / BNC**: PULSE_OUT_0..3 (PC6, PC7, PC8, PC9). 50 Ω trace from
  pin to connector. See "Output drive" below — a direct CMOS pin into a
  BNC is rarely what you want for ≤250 ps edges.
- **USB-C / micro-USB** for PA11/PA12 and 5 V supply input.
- **3-pin debug UART header**: GND, PA2 (TX from MCU), PA3 (RX into MCU).
  1 Mbaud in firmware. Optional level shifter / FTDI-style adapter on the
  cable side.
- **SWD**: PA13 (SWDIO), PA14 (SWCLK), NRST, GND, 3V3.
- **10 MHz clock input**: SMA / U.FL into PF0 (OSC_IN). With HSE bypass
  the internal amplifier is off, so this is a single-ended logic input,
  not a crystal connection. Series cap + DC-block on the input is fine.

## Power

- USB 5 V → **AP7375-33** (300 mA) → 3V3 rail.
- 1 × 4.7 µF + 1 × 100 nF on each VDD pin pair.
- **VCAP**: 2.2 µF ceramic, X7R, GND-referenced. (Required on G474; the
  G431 had this internal.)
- **VDDA** through a ferrite bead from VDD with 1 µF + 100 nF bypass.

## Output drive

**Assumption: every cable has a 50 Ω termination at the far end** (a
50 Ω scope input, a counter, a comparator with built-in termination).
With proper far-end termination the line is back-terminated; any wave
launched down it is absorbed at the load and there are no reflections
to worry about. That removes the main reason a buffer existed.

So: **drive the output pin directly into the BNC**. No series resistor, no
buffer chip. Just route a 50 Ω trace from the MCU pin to the connector
and let the load do the termination.

What you give up:

- **Swing.** G474 GPIO output impedance is ~25 Ω in "very high" speed
  mode. Into a 50 Ω termination that's a divider: `3.3 V × 50/(25+50) ≈
  2.2 V` at the receiver. Plenty for any 50 Ω scope / counter / TTL
  trigger; not enough if the receiver wants 5 V CMOS levels.
- **Edge rate.** Still GPIO-limited, ~1–2 ns rise/fall. HRTIM places
  the edge *timing* with 250 ps precision, but the edge itself crosses
  the trigger threshold over ~1 ns. For most "trigger an instrument"
  or "fire a laser-diode driver" use cases, edge timing is what
  matters and edge rate is not.

If you ever need sub-ns edges (no-hysteresis comparators,
time-of-flight, etc.), reach for a dedicated driver like **NB6L11**
(LVPECL, ~50 ps edges) or **NB7L86** (~70 ps clock fanout). Those
need dual supplies and are overkill if the receiver will retime.

If you sometimes need to drive a high-Z load instead of a 50 Ω one,
you'll see ringing — either add an outboard 50 Ω feed-through
terminator at the receiver, or switch to source termination (51 Ω
series at the MCU pin) and accept the swing at the high-Z receiver
that comes with it.

## Input clock

The 10 MHz HSE source (rubidium, GPS-disciplined OCXO, etc.) drives PF0
through HSE bypass. Logic-level square wave is what the silicon expects.
If your reference is a sine wave, AC-couple through a fast comparator or
a CMOS Schmitt buffer first.

If HSE is missing at boot, firmware falls back to HSI and refuses to
output anything (`g_rulos_hse_failed` path). LED_CLOCK goes dark, the
channel LEDs flash in unison, and a `# FATAL` message goes out the USB
port.

## Footnotes

- The G474 LQFP32 package does **not** exist; LQFP48 is the smallest
  variant that gives you PF1, and LQFP64 is needed for Port C (PC6-PC9).
- Each TIM3 channel has its own CCR, but TIM3 has a single shared ARR
  and a single shared 16-bit prescaler. Two channels in TIM3 mode at
  the same time will share whichever period was set last. Widths are
  per-channel and unaffected.
- USB DFU is enabled automatically by the `usb-cdc-stm32` peripheral
  (the composite descriptor adds a DFU runtime interface). VID/PID
  `0x1209:0x71C5` is configured in SConstruct so `dfu-util -d 1209:71C5
  -a 0 -s 0x08000000:leave -D pulsegen.bin` will update firmware in
  one command.
