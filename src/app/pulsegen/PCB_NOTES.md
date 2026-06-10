# Pulsegen PCB notes

Hardware target: **STM32G474RE** (LQFP64, `stm32g474xe` in the build
system: 512 KB flash, 96 KB SRAM). LQFP64 is needed for Port C access.

---

# Next revision — what to verify before laying out the board

**Goal the pinout must support:** 4 independently-programmable periods,
AND a SYNC mode where all four phase-lock to one shared period spanning
~24 ns to ~30 s (HRTIM hands off to a GP timer above HRTIM's ~524 µs
ceiling). The current PC6-9 pinout can't: its outputs reach only HRTIM
Timers E/F (2 timers → 2 shared periods) and only TIM3 as a GP fallback
(one shared ARR → one long period for all four).

The list below is the actual work for the next rev: each item is a
datasheet/RM lookup to do **before committing the layout.** Candidate
pinout the checks refer to:

| Ch | Pin  | HRTIM (fast)   | GP timer (slow)            |
|----|------|----------------|----------------------------|
| 0  | PA9  | Timer A, CHA2  | TIM2_CH3 (TIM2 is 32-bit)  |
| 1  | PA10 | Timer B, CHB1  | TIM1_CH3                   |
| 2  | PC6  | Timer F, CHF1  | TIM3_CH1                   |
| 3  | PC8  | Timer E, CHE1  | TIM8_CH3                   |

## The checks

1. **Each pin is on a *distinct* HRTIM timer.** Confirm the four outputs
   land on four *different* HRTIM timers (A/B/E/F above). Two outputs on
   the same HRTIM timer share one counter/period — the current board's
   defect that caps it at 2 independent periods. [DS12288 pin/AF table.]

2. **Each pin *also* reaches a *distinct* GP timer.** Confirm every pin
   has a GP/advanced-timer channel too, on four *different* timers
   (TIM1/2/3/8), so the long periods are independent. Confirm one is
   32-bit (TIM2) for the longest periods. [DS12288 pin/AF table.]

3. **Those four GP timers can be mutually synchronized.** Confirm one of
   them can reset/trigger the other three through the timer interconnect,
   or SYNC breaks at long periods. On G4: TIM1 master, TIM2/TIM3/TIM8
   slave on ITR0 (= tim1_trgo). [RM0440 Table 307, the unified ITR mux —
   re-confirm for the exact four timers chosen.]

4. **Each pin's alternate-function number — looked up INDIVIDUALLY.** This
   is the bug that cost us a board generation. HRTIM outputs are NOT all
   on the same AF: on this chip PC6 CHF1 = **AF13** but PC8 CHE1 = **AF3**.
   The firmware hardcoded AF13 for all four, so the Timer-E channels
   emitted nothing — undetected until a 4-channel test finally drove them.
   Verify EACH pin's HRTIM AF and EACH pin's GP-timer AF as separate
   lookups (and keep them in a per-channel firmware table, not a shared
   constant). Status:
   - PC6 CHF1 = AF13, PC8 CHE1 = AF3 — **confirmed.**
   - PA9 CHA2, PA10 CHB1 HRTIM AFs — **unverified, look up.**
   - all four GP-timer-channel AFs — **unverified, look up.**
   [DS12288 alternate-function table.]

5. **No collision with the fixed-function pins.** PA9/PA10 are currently
   the debug UART (USART1) — relocate it to USART2 (PA2/PA3), as the
   2-channel prototype did. Confirm still clear: PA11/12 = USB, PA13/14 =
   SWD, PF0 = 10 MHz HSE.

6. **Package exposes every pin.** LQFP64 (Port C is needed for PC6/PC8;
   LQFP48 has none). Confirm the chosen variant brings out all of the
   above.

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

# Current board (as built, STM32G474RE / PC6-9)

## Critical pin choices

These are load-bearing for the firmware as written; changing them means
changing code in `pulsegen.c`.

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
long periods anyway — that's a next-board goal). So the current board's
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
