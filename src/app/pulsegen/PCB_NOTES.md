# Pulsegen PCB notes

Hardware target: **STM32G474xB** (LQFP48 minimum, for PF1 access).

## Critical pin choices

These are load-bearing for the firmware as written; changing them means
changing code in `pulsegen.c`.

| Net          | MCU pin | Why                                                          |
|--------------|---------|--------------------------------------------------------------|
| PULSE_OUT_1  | **PA9** | Only G474 pin that maps to both HRTIM (CHA2 / AF13) and a 32-bit timer (TIM2_CH3 / AF10). The firmware switches AF at runtime. |
| PULSE_OUT_2  | **PA10**| Same property: HRTIM_CHB1 (AF13) and TIM2_CH4 (AF10).        |
| OSC_IN       | PF0     | HSE input, fed by the 10 MHz active oscillator.              |
| OSC_OUT (PF1)| LED_CLOCK | HSE bypass mode frees PF1; we drive a heartbeat LED here. |
| USB_DM       | PA11    | Fixed by silicon.                                            |
| USB_DP       | PA12    | Fixed by silicon.                                            |
| UART_TX      | PA2     | USART2_TX, AF7 (rulos `uart_id=1`).                          |
| UART_RX      | PA3     | USART2_RX, AF7.                                              |

The USART1 pins (PA9/PA10) are reserved for outputs, which is why the
debug UART has to live on USART2.

### LEDs (flexible — change in `pulsegen.c` if you reroute)

| Function       | Default pin |
|----------------|-------------|
| Channel 1 act. | PA4         |
| Channel 2 act. | PA5         |
| HSE heartbeat  | PF1         |
| USB enumerated | PA8         |

## Headers

- **2× SMA / BNC**: PULSE_OUT_1 (PA9), PULSE_OUT_2 (PA10). 50 Ω trace from
  pin to connector. See "Output buffer" below — a direct CMOS pin into a
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

So: **drive PA9/PA10 directly into the BNC**. No series resistor, no
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
  variant that gives you PF1.
- `PA8` is HRTIM1_CHA1 — kept free as a USB activity LED rather than
  routed to a connector. If you ever want a 3rd output channel, PA8 is
  HRTIM-only (no 32-bit-timer fallback), so it's only useful for periods
  ≤524 µs unless you accept losing the long-period mode for that channel.
