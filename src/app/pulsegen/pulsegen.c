/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.  If
 * not, see <https://www.gnu.org/licenses/>.
 */

/*
 * pulsegen (PG-4 Rev B): a 4-channel pulse generator on the STM32G474, built around the
 * High-Resolution Timer (HRTIM) for fine edge placement and the general-purpose timers for long
 * periods. Configuration is delivered over USB CDC using a small subset of SCPI; a UART debug port
 * stays available for logs.
 *
 * Clock plan (set in SConstruct via PLLM/PLLN):
 *   HSE 10 MHz / PLLM=1 * PLLN=25 / PLLR=2 = 125 MHz SYSCLK
 *   fHRTIM = 125 MHz; with the HRTIM DLL at 32x oversampling, fHRCK = 4 GHz and the finest tick is
 *   250 ps. The general-purpose timers run at 125 MHz -> 8 ns tick.
 *
 * WHAT REV B FIXES vs REV A. The old board routed all four outputs to HRTIM Timers E and F only
 * (two timers, two shared periods), so four independent periods were physically impossible. Rev B
 * routes each channel to its OWN HRTIM timer AND its OWN general-purpose timer, so every channel
 * is fully independent across the whole 24 ns - 34 s range:
 *
 *   Ch  Pin   HRTIM (fast, 250 ps)     GP timer (slow, 8 ns)
 *   0   PC8   Timer E, CHE1   AF3      TIM8 CH3 [+CH4]           AF4
 *   1   PC6   Timer F, CHF1   AF13     TIM3 CH1 [+CH2]           AF2
 *   2   PA9   Timer A, CHA2   AF13     TIM2 (32-bit) CH3 [+CH4]  AF10
 *   3   PA10  Timer B, CHB1   AF13     TIM1 CH3 [+CH4]           AF6
 *
 * Every AF above is verified against DS12288 Rev 6 Table 13; note the HRTIM AF is NOT uniform
 * (Timer E / PC8 is AF3, the rest AF13) and the GP AF differs on every pin, so AF is a per-channel
 * field, never a constant. See PCB_NOTES.md for the full verification.
 *
 * TIMER MAP (every hardware timer this firmware touches):
 *   HRTIM1 master    fast-regime sync timebase; also the burst-mode-controller clock in SYNC
 *   HRTIM1 Timer A   ch2 fast output (PA9)
 *   HRTIM1 Timer B   ch3 fast output (PA10)
 *   HRTIM1 Timer E   ch0 fast output (PC8)
 *   HRTIM1 Timer F   ch1 fast output (PC6)
 *   HRTIM1 BMC       burst gating in the fast regime (hardware-exact pulse count)
 *   TIM1             ch3 slow (GP) output, and the GP-regime sync master (TRGO = update)
 *   TIM2 (32-bit)    ch2 slow (GP) output
 *   TIM3             ch1 slow (GP) output
 *   TIM8             ch0 slow (GP) output
 *   TIM5 (32-bit)    burst-repetition timer: times the gap between burst frames; drives no pin
 *   SysTick          RULOS scheduler jiffy (LEDs, SCPI, housekeeping) -- never time-critical
 * A channel's HRTIM timer and GP timer are mutually exclusive in time (the pin's AF switches at the
 * ~524 us crossover), so the two are never driving the same output at once.
 *
 * DESIGN RULE: full feature parity across the two regimes. The handoff between HRTIM and the GP
 * timer is invisible -- you set a period anywhere in the range and the firmware picks the timer.
 * Width, delay, sync, and burst all behave identically on both sides. The ONLY thing that changes
 * at the ~524 us crossover is edge-placement granularity (250 ps below, 8 ns above), a precision
 * detail (8 ns is ~15 ppm at 524 us), not a behavior change.
 *
 *   Per-channel regime: period <= HRTIM ceiling (~524 us) -> HRTIM; above -> the channel's GP
 *   timer. The pin's alternate function is switched HRTIM-AF <-> GP-AF at the crossover.
 *
 *   ASYNC mode (default): four fully independent periods; each channel picks its own regime.
 *   SYNC mode: all four channels share one period and phase; a per-channel delay places each
 *     rising edge relative to a common t=0. Delay is meaningful only in SYNC -- async channels
 *     free-run with no shared reference -- which is a mode property, uniform across the range.
 *
 * HRTIM output: each channel is alone on its HRTIM timer, so it uses CMP1 (rising edge, at delay)
 * and CMP2 (falling edge, at delay+width); a ~0 delay sets the output on the period event instead.
 *
 * GP output: a single GP-timer channel has one compare = one edge, so an arbitrary [delay,
 * delay+width] pulse is built with Combined PWM mode, which gangs the channel's compare with its
 * sibling's onto one output. The routed channel runs Combined PWM mode 2 (its ref = high from CCR
 * to ARR, CCR = delay) and the sibling runs PWM mode 1 (high from 0 to CCR, CCR = delay+width);
 * the AND of the two is high only in [delay, delay+width]. The sibling's own output is not enabled
 * (so e.g. ch1's TIM1_CH4 = PA11 stays USB). Advanced timers (TIM1/TIM8) need BDTR.MOE.
 *
 * SYNC master follows the regime: in the HRTIM regime the HRTIM master resets all slave timers
 * each period; in the GP regime TIM1 is master (TRGO = update) and TIM2/TIM3/TIM8 slave-reset on
 * ITR0 = tim1_trgo. Either way all counters are preset to 0 before a single coordinated enable, so
 * they start phase-aligned.
 *
 * Burst: a domain can emit exactly N pulses, rest, and repeat at a programmable interval. Same
 * behavior in both regimes, two implementations: the HRTIM burst-mode controller gates the outputs
 * in hardware when fast (software can't count MHz pulses), and a software per-period count gates
 * them when slow (the update interrupt runs below ~2 kHz there, so counting is exact). The
 * repetition interval is timed by a dedicated hardware timer (TIM5), not the scheduler, so the
 * cadence is locked to the 125 MHz clock -- phase-coherent with the pulses and free of scheduler
 * jitter; the count within a burst stays exact.
 *
 * SCPI command surface:
 *   *IDN?
 *   *RST
 *   MODE ASYNC|SYNC                         (MODE? to query)
 *   OUTPut<n>:STATe ON|OFF
 *   SOURce<n>:PULSe:PERiod  <seconds>       per channel (async) / global (sync)
 *   SOURce<n>:PULSe:WIDTh   <seconds>       per channel
 *   SOURce<n>:PULSe:DELay   <seconds>       per-channel rising-edge offset (SYNC mode)
 *   SOURce<n>:BURSt:STATe ON|OFF
 *   SOURce<n>:BURSt:NCYCles <count>
 *   SOURce<n>:BURSt:INTernal:PERiod <sec>
 *   SYSTem:ERRor?
 * The three BURSt commands also have query forms (append '?').
 *
 * Reconfiguration tears down and rebuilds the whole timer state on any change, so changing one
 * channel briefly glitches the others -- fine for a bench source that is set up and then observed.
 *
 * Firmware update over USB DFU (no SWD probe needed):
 *   sudo dfu-util -d 1209:71c5,0483:df11 -a 0 -s 0x08000000:leave -D <fw>.bin
 */

#include "pulsegen.h"

#include <stdbool.h>
#include <stdint.h>

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/scpi/scpi.h"
#include "periph/uart/uart.h"
#include "scpi.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_hrtim.h"
#include "stm32g4xx_ll_tim.h"

#ifndef RULOS_USE_HSE
#error "pulsegen requires RULOS_USE_HSE"
#endif

// NUM_CHANNELS comes from pulsegen.h.

#define PS_PER_SEC 1000000000000ULL

// ---- HRTIM (fast) regime ---------------------------------------------------

// fHRCK at CKPSC=0 = 32 * 125 MHz = 4 GHz -> 250 ps/tick.
#define HRTIM_FINEST_TICK_PS 250ULL
// HRTIM 16-bit period max in high-res mode (RM0440 27.3.4): at most 0xFFDF.
#define HRTIM_PER_MAX 0xFFDFU

static const uint32_t hrtim_ckpsc_to_ll[6] = {
    LL_HRTIM_PRESCALERRATIO_MUL32, LL_HRTIM_PRESCALERRATIO_MUL16, LL_HRTIM_PRESCALERRATIO_MUL8,
    LL_HRTIM_PRESCALERRATIO_MUL4,  LL_HRTIM_PRESCALERRATIO_MUL2,  LL_HRTIM_PRESCALERRATIO_DIV1,
};

// Per-CKPSC minimum PER value (RM0440 Table 218). Also the minimum usable compare value: a delay
// below this many ticks is treated as zero (set on the period event instead).
static const uint16_t hrtim_min_per[6] = {0x0060, 0x0030, 0x0018, 0x000C, 0x0006, 0x0003};

// Longest period the HRTIM can express: CKPSC=5 (8 ns tick) * 0xFFDF ~= 524 us. Periods above this
// hand off to the channel's GP timer.
#define HRTIM_MAX_PERIOD_PS (((uint64_t)HRTIM_PER_MAX) * (HRTIM_FINEST_TICK_PS << 5))

// ---- GP (slow) regime ------------------------------------------------------

// General-purpose timers run at 125 MHz -> 8 ns/tick (CK_INT, APBx prescaler = 1).
#define GP_TICK_PS   8000ULL
#define GP_PSC_MAX   0xFFFFU      // 16-bit prescaler on every GP timer
#define GP_ARR16_MAX 0xFFFFU      // 16-bit timers (TIM1/3/8)
#define GP_ARR32_MAX 0xFFFFFFFFU  // 32-bit timer (TIM2)
// Longest period offered on every channel, capped to the 16-bit timers' reach so all four behave
// identically (TIM2's 32-bit counter could go further, but uniformity wins): 65536 * 65536 * 8 ns.
#define GP_MAX_PERIOD_PS (((uint64_t)GP_ARR16_MAX + 1) * (GP_PSC_MAX + 1) * GP_TICK_PS)

// ---- Burst -----------------------------------------------------------------

// Each HRTIM burst frame is a short idle window followed by ncyc pulse periods, both counted by the
// BMC's 16-bit period register (so idle + ncyc <= 65536). The idle window holds the outputs off
// while burst_arm_frame enables them and -- because the controller runs continuous and is stopped
// in the BMPER ISR -- it is the grace period for that ISR to disable the outputs before the next
// idle window would end and they'd resume. It is therefore sized by absolute time (worst-case ISR
// latency), not a fixed period count, so it does not balloon into seconds at slow periods.
#define BURST_IDLE_TARGET_PS (200ULL * 1000 * 1000)  // ~200 us: BMPER-ISR latency budget + arm
#define BURST_IDLE_MIN       2U                      // period floor (near-ceiling periods only)
#define BURST_NCYC_MAX       (65536U - BURST_IDLE_MIN)

#define BURST_REP_DEFAULT_PS PS_PER_SEC
#define BURST_REP_MARGIN_PS  (PS_PER_SEC / 1000)   // 1 ms guard past the frame for ISR latency
#define BURST_REP_MAX_PS     (60ULL * PS_PER_SEC)  // TIM5 32-bit reach (16 ns tick at div 2)

// ---- LEDs (unchanged from Rev A; none collide with the Rev B outputs) ------

#define LED_CHAN0 GPIO_C0
#define LED_CHAN1 GPIO_A3
#define LED_CHAN2 GPIO_A4
#define LED_CHAN3 GPIO_A8
#define LED_CLOCK GPIO_B4  // 10 MHz HSE health
#define LED_USB   GPIO_A5

// ---- Channel hardware mapping ----------------------------------------------

// Each channel owns one HRTIM timer (one output) and one GP timer. In the GP regime the routed
// output channel is paired with a sibling channel to form a Combined-PWM pulse; the sibling's own
// output is never enabled.
typedef struct {
  GPIO_TypeDef* gpio;     // GPIOA or GPIOC
  uint32_t ll_pin;        // LL_GPIO_PIN_n
  uint8_t hrtim_af;       // per-pin HRTIM AF (AF13, except PC8 = AF3)
  uint32_t hrtim_timer;   // LL_HRTIM_TIMER_A/B/E/F
  uint32_t hrtim_output;  // LL_HRTIM_OUTPUT_Txy
  TIM_TypeDef* gp_timer;  // TIM1/TIM2/TIM3/TIM8
  uint32_t gp_out_ch;     // LL_TIM_CHANNEL_CHn (the routed output)
  uint32_t gp_sib_ch;     // LL_TIM_CHANNEL_CHn (sibling, internal, for the 2nd edge)
  uint8_t gp_af;          // per-pin GP-timer AF
  bool gp_advanced;       // TIM1/TIM8 need BDTR.MOE
  IRQn_Type gp_up_irqn;   // update IRQ, for software-counted GP burst
  bool gp_32bit;          // TIM2 has a 32-bit counter
  gpio_pin_t led;
} channel_hw_t;

// Channel -> pin assignment is the Rev B layout (driven by board routing); the per-pin hardware
// (HRTIM timer/output/AF, GP timer/channel/AF) is fixed by the silicon, verified in PCB_NOTES.
//   ch0 = PC8 (Timer E / TIM8), ch1 = PC6 (Timer F / TIM3),
//   ch2 = PA9 (Timer A / TIM2), ch3 = PA10 (Timer B / TIM1).
static const channel_hw_t channel_hw[NUM_CHANNELS] = {
    [0] = {.gpio = GPIOC,
           .ll_pin = LL_GPIO_PIN_8,
           .hrtim_af = 3,
           .hrtim_timer = LL_HRTIM_TIMER_E,
           .hrtim_output = LL_HRTIM_OUTPUT_TE1,
           .gp_timer = TIM8,
           .gp_out_ch = LL_TIM_CHANNEL_CH3,
           .gp_sib_ch = LL_TIM_CHANNEL_CH4,
           .gp_af = 4,
           .gp_advanced = true,
           .gp_up_irqn = TIM8_UP_IRQn,
           .gp_32bit = false,
           .led = LED_CHAN0},
    [1] = {.gpio = GPIOC,
           .ll_pin = LL_GPIO_PIN_6,
           .hrtim_af = 13,
           .hrtim_timer = LL_HRTIM_TIMER_F,
           .hrtim_output = LL_HRTIM_OUTPUT_TF1,
           .gp_timer = TIM3,
           .gp_out_ch = LL_TIM_CHANNEL_CH1,
           .gp_sib_ch = LL_TIM_CHANNEL_CH2,
           .gp_af = 2,
           .gp_advanced = false,
           .gp_up_irqn = TIM3_IRQn,
           .gp_32bit = false,
           .led = LED_CHAN1},
    [2] = {.gpio = GPIOA,
           .ll_pin = LL_GPIO_PIN_9,
           .hrtim_af = 13,
           .hrtim_timer = LL_HRTIM_TIMER_A,
           .hrtim_output = LL_HRTIM_OUTPUT_TA2,
           .gp_timer = TIM2,
           .gp_out_ch = LL_TIM_CHANNEL_CH3,
           .gp_sib_ch = LL_TIM_CHANNEL_CH4,
           .gp_af = 10,
           .gp_advanced = false,
           .gp_up_irqn = TIM2_IRQn,
           .gp_32bit = true,
           .led = LED_CHAN2},
    [3] = {.gpio = GPIOA,
           .ll_pin = LL_GPIO_PIN_10,
           .hrtim_af = 13,
           .hrtim_timer = LL_HRTIM_TIMER_B,
           .hrtim_output = LL_HRTIM_OUTPUT_TB1,
           .gp_timer = TIM1,
           .gp_out_ch = LL_TIM_CHANNEL_CH3,
           .gp_sib_ch = LL_TIM_CHANNEL_CH4,
           .gp_af = 6,
           .gp_advanced = true,
           .gp_up_irqn = TIM1_UP_TIM16_IRQn,
           .gp_32bit = false,
           .led = LED_CHAN3},
};

// The GP-regime sync master is whichever channel owns TIM1 (it emits the TRGO the other GP timers
// slave to). Found by table, not hardcoded, so a channel<->pin remap can't desync it.
static int gp_master_ch(void) {
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    if (channel_hw[ch].gp_timer == TIM1) {
      return ch;
    }
  }
  return -1;
}

// ---- Channel runtime state -------------------------------------------------

typedef enum {
  MODE_ASYNC = 0,  // four independent periods (default)
  MODE_SYNC,       // all channels share one period, phase-locked
} pulsegen_mode_t;

// All mutable per-channel state, one instance per channel (the fixed hardware mapping lives
// separately in channel_hw[]). Times are picoseconds. In sync mode every channel's period is kept
// equal, so each always reads back the effective period.
typedef struct {
  uint64_t period_ps;
  uint64_t width_ps;
  uint64_t delay_ps;  // rising-edge offset
  bool on;

  bool burst_on;
  uint32_t burst_ncyc;
  uint64_t burst_rep_ps;

  // LED: when led_per_pulse, the LED is toggled (led_state) once per pulse by the GP update ISR
  // rather than blinking at the heartbeat (see the LED policy note below).
  bool led_per_pulse;
  bool led_state;
} channel_state_t;

static channel_state_t chan[NUM_CHANNELS];

static pulsegen_mode_t g_mode = MODE_ASYNC;

// A channel's domain is itself in async (independent periods), the whole device in sync.
static bool same_domain(int a, int b) {
  return g_mode == MODE_SYNC || a == b;
}

// True iff this channel runs in the HRTIM (fast) regime at its current period.
static bool uses_hrtim(int ch) {
  return chan[ch].period_ps <= HRTIM_MAX_PERIOD_PS;
}

// ---- Globals ---------------------------------------------------------------

static UartState_t uart;
static bool hrtim_dll_ready = false;

// Active HRTIM burst state, owned by apply_all().
static uint32_t g_hrtim_burst_outputs;  // OENR/ODISR mask; 0 = no active HRTIM burst
// Active GP burst state: count the update events of g_gp_burst_count_ch's timer (the bursting
// domain's timebase -- TIM1 in sync, the single channel in async), and gate every channel in
// g_gp_burst_out_mask on period boundaries so the run window holds exactly ncyc whole pulses.
static int g_gp_burst_count_ch = -1;
static uint32_t g_gp_burst_out_mask;
static volatile uint32_t g_gp_burst_remaining;
static volatile bool g_gp_burst_arming;
// A burst (either regime) is armed this config; TIM5's update ISR re-arms each frame while set.
static bool g_burst_active;

// Channel-LED policy. A fast channel (period < LED_SLOW_PERIOD_PS, i.e. freq > 2 Hz -- always the
// case in the HRTIM regime) blinks at the fixed heartbeat in update_leds. A slow continuous channel
// (freq <= 2 Hz, always the GP regime) instead toggles its LED once per pulse, off the GP timer's
// update (rollover) interrupt -- so you can see each slow pulse. Bursting channels use the
// heartbeat (their GP update IRQ, if any, is owned by burst counting). State is per-channel:
// chan[].led_per_pulse / chan[].led_state.
#define LED_SLOW_PERIOD_PS (PS_PER_SEC / 2)  // 0.5 s = 2 Hz

// ---- Small helpers ---------------------------------------------------------

static void set_pin_af(int ch, uint8_t af) {
  const channel_hw_t* hw = &channel_hw[ch];
  if (hw->ll_pin <= LL_GPIO_PIN_7) {
    LL_GPIO_SetAFPin_0_7(hw->gpio, hw->ll_pin, af);
  } else {
    LL_GPIO_SetAFPin_8_15(hw->gpio, hw->ll_pin, af);
  }
  LL_GPIO_SetPinMode(hw->gpio, hw->ll_pin, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetPinSpeed(hw->gpio, hw->ll_pin, LL_GPIO_SPEED_FREQ_VERY_HIGH);
  LL_GPIO_SetPinOutputType(hw->gpio, hw->ll_pin, LL_GPIO_OUTPUT_PUSHPULL);
  LL_GPIO_SetPinPull(hw->gpio, hw->ll_pin, LL_GPIO_PULL_NO);
}

static void drive_pin_low(int ch) {
  const channel_hw_t* hw = &channel_hw[ch];
  LL_GPIO_ResetOutputPin(hw->gpio, hw->ll_pin);
  LL_GPIO_SetPinMode(hw->gpio, hw->ll_pin, LL_GPIO_MODE_OUTPUT);
  LL_GPIO_SetPinOutputType(hw->gpio, hw->ll_pin, LL_GPIO_OUTPUT_PUSHPULL);
}

// Set a GP timer's compare register by channel mask (LL splits these per channel number).
static void gp_set_compare(TIM_TypeDef* t, uint32_t ll_channel, uint32_t value) {
  switch (ll_channel) {
    case LL_TIM_CHANNEL_CH1:
      LL_TIM_OC_SetCompareCH1(t, value);
      break;
    case LL_TIM_CHANNEL_CH2:
      LL_TIM_OC_SetCompareCH2(t, value);
      break;
    case LL_TIM_CHANNEL_CH3:
      LL_TIM_OC_SetCompareCH3(t, value);
      break;
    default:
      LL_TIM_OC_SetCompareCH4(t, value);
      break;
  }
}

// ---- HRTIM (fast) regime ---------------------------------------------------

// Pick the finest CKPSC whose period fits the 16-bit HRTIM period register. Caller has already
// established period_ps <= HRTIM_MAX_PERIOD_PS.
static bool select_ckpsc(uint64_t period_ps, uint8_t* out_ckpsc, uint32_t* out_per) {
  for (uint8_t k = 0; k <= 5; k++) {
    uint64_t tick_ps = HRTIM_FINEST_TICK_PS << k;
    uint64_t per = period_ps / tick_ps;
    if (per > HRTIM_PER_MAX || per < hrtim_min_per[k]) {
      continue;
    }
    *out_ckpsc = k;
    *out_per = (uint32_t)per;
    return true;
  }
  return false;
}

// Configure one channel's HRTIM timer (counter) and its single output. sync => reset on the HRTIM
// master period so the timer phase-locks to the others. burst => the output is gated by the burst
// mode controller and left disabled here (the repetition task enables it per burst).
static void config_hrtim_channel(int ch, uint8_t ckpsc, uint32_t per, bool sync, bool burst) {
  const channel_hw_t* hw = &channel_hw[ch];
  const uint32_t timer = hw->hrtim_timer;
  const uint64_t tick_ps = HRTIM_FINEST_TICK_PS << ckpsc;
  const uint32_t min_t = hrtim_min_per[ckpsc];

  LL_HRTIM_TIM_SetPrescaler(HRTIM1, timer, hrtim_ckpsc_to_ll[ckpsc]);
  LL_HRTIM_TIM_SetCounterMode(HRTIM1, timer, LL_HRTIM_MODE_CONTINUOUS);
  LL_HRTIM_TIM_SetPeriod(HRTIM1, timer, per);
  LL_HRTIM_TIM_SetRepetition(HRTIM1, timer, 0);
  LL_HRTIM_TIM_SetCountingMode(HRTIM1, timer, LL_HRTIM_COUNTING_MODE_UP);
  LL_HRTIM_TIM_DisablePreload(HRTIM1, timer);
  LL_HRTIM_TIM_SetResetTrig(HRTIM1, timer,
                            sync ? LL_HRTIM_RESETTRIG_MASTER_PER : LL_HRTIM_RESETTRIG_NONE);

  uint32_t delay_t = (uint32_t)(chan[ch].delay_ps / tick_ps);
  // A bursting output must not set on the period event (it races the burst controller's idle
  // release); clamp the rising edge to the first usable compare value instead.
  if (burst && delay_t < min_t) {
    delay_t = min_t;
  }
  uint32_t width_t = (uint32_t)(chan[ch].width_ps / tick_ps);
  if (width_t < min_t) {
    width_t = min_t;
  }
  uint32_t fall_t = delay_t + width_t;
  if (fall_t > per - 1) {
    fall_t = per - 1;
  }
  const bool zero_delay = (delay_t < min_t);

  if (!zero_delay) {
    LL_HRTIM_TIM_SetCompare1(HRTIM1, timer, delay_t);
  }
  LL_HRTIM_TIM_SetCompare2(HRTIM1, timer, fall_t);

  const uint32_t out = hw->hrtim_output;
  LL_HRTIM_OUT_SetPolarity(HRTIM1, out, LL_HRTIM_OUT_POSITIVE_POLARITY);
  LL_HRTIM_OUT_SetOutputSetSrc(HRTIM1, out,
                               zero_delay ? LL_HRTIM_OUTPUTSET_TIMPER : LL_HRTIM_OUTPUTSET_TIMCMP1);
  LL_HRTIM_OUT_SetOutputResetSrc(HRTIM1, out, LL_HRTIM_OUTPUTRESET_TIMCMP2);
  LL_HRTIM_OUT_SetIdleMode(HRTIM1, out,
                           burst ? LL_HRTIM_OUT_IDLE_WHEN_BURST : LL_HRTIM_OUT_NO_IDLE);
  LL_HRTIM_OUT_SetIdleLevel(HRTIM1, out, LL_HRTIM_OUT_IDLELEVEL_INACTIVE);
  LL_HRTIM_OUT_SetFaultState(HRTIM1, out, LL_HRTIM_OUT_FAULTSTATE_NO_ACTION);
  LL_HRTIM_OUT_SetChopperMode(HRTIM1, out, LL_HRTIM_OUT_CHOPPERMODE_DISABLED);

  set_pin_af(ch, hw->hrtim_af);
  if (!burst) {
    LL_HRTIM_EnableOutput(HRTIM1, out);
  }
}

// ---- GP (slow) regime ------------------------------------------------------

// Pick the smallest prescaler whose period fits in GP_MAX_PERIOD_PS worth of 16-bit-counter ticks
// (uniform across channels). out_psc is the PSC register value (divisor - 1); out_arr is the ARR
// value (period in prescaled ticks - 1). Computed directly, no scan.
static bool select_gp_psc(int ch, uint64_t period_ps, uint32_t* out_psc, uint32_t* out_arr,
                          uint64_t* out_tick_ps) {
  // div = ceil(period_ps / (GP_TICK_PS * (GP_ARR16_MAX + 1))): the fewest prescaled ticks need
  // (GP_ARR16_MAX + 1) of them at most.
  const uint64_t span = GP_TICK_PS * (GP_ARR16_MAX + 1);
  uint64_t div = (period_ps + span - 1) / span;
  if (div < 1) {
    div = 1;
  }
  if (div > GP_PSC_MAX + 1) {
    return false;  // longer than GP_MAX_PERIOD_PS
  }
  uint64_t tick_ps = GP_TICK_PS * div;
  uint64_t ticks = period_ps / tick_ps;
  if (ticks < 2) {
    return false;  // shorter than the GP timer can express -- caller should have used HRTIM
  }
  *out_psc = (uint32_t)(div - 1);
  *out_arr = (uint32_t)(ticks - 1);
  *out_tick_ps = tick_ps;
  return true;
}

// Configure a GP timer's counter (mode / prescaler / ARR) and its master-or-slave role for the
// given period. TIM1 is always the GP-regime sync master (TRGO = update, drives the slaves' ITR0);
// the others slave-reset off it when sync, else free-run. Split out from the output stage so the
// master timebase can run even when its own channel (ch1) is off. Returns ARR + tick via out
// params; false if the period doesn't fit.
static bool config_gp_timebase(int ch, uint64_t period_ps, bool sync, uint32_t* out_arr,
                               uint64_t* out_tick_ps) {
  TIM_TypeDef* t = channel_hw[ch].gp_timer;
  uint32_t psc, arr;
  uint64_t tick_ps;
  if (!select_gp_psc(ch, period_ps, &psc, &arr, &tick_ps)) {
    return false;
  }
  LL_TIM_SetCounterMode(t, LL_TIM_COUNTERMODE_UP);
  LL_TIM_SetPrescaler(t, psc);
  LL_TIM_SetAutoReload(t, arr);
  LL_TIM_DisableARRPreload(t);
  if (t == TIM1) {
    LL_TIM_SetTriggerOutput(t, LL_TIM_TRGO_UPDATE);
    LL_TIM_SetSlaveMode(t, LL_TIM_SLAVEMODE_DISABLED);
  } else {
    LL_TIM_SetTriggerInput(t, LL_TIM_TS_ITR0);
    LL_TIM_SetSlaveMode(t, sync ? LL_TIM_SLAVEMODE_RESET : LL_TIM_SLAVEMODE_DISABLED);
  }
  *out_arr = arr;
  *out_tick_ps = tick_ps;
  return true;
}

// Configure a channel's Combined-PWM output (timebase already set up by config_gp_timebase). The
// output channel's ref is PWM mode 2 (high CCR..ARR, CCR = delay); the sibling is PWM mode 1 (high
// 0..CCR, CCR = delay+width); the AND is high only in [delay, delay+width]. burst leaves the
// output channel disabled (the burst gating enables it on a period boundary).
static void config_gp_output(int ch, uint32_t arr, uint64_t tick_ps, bool burst) {
  const channel_hw_t* hw = &channel_hw[ch];
  TIM_TypeDef* t = hw->gp_timer;
  uint32_t delay_t = (uint32_t)(chan[ch].delay_ps / tick_ps);
  uint32_t width_t = (uint32_t)(chan[ch].width_ps / tick_ps);
  if (width_t < 1) {
    width_t = 1;
  }
  uint32_t fall_t = delay_t + width_t;
  if (fall_t > arr) {
    fall_t = arr;
  }
  LL_TIM_OC_SetMode(t, hw->gp_out_ch, LL_TIM_OCMODE_COMBINED_PWM2);
  LL_TIM_OC_SetMode(t, hw->gp_sib_ch, LL_TIM_OCMODE_PWM1);
  gp_set_compare(t, hw->gp_out_ch, delay_t);
  gp_set_compare(t, hw->gp_sib_ch, fall_t);
  LL_TIM_OC_SetPolarity(t, hw->gp_out_ch, LL_TIM_OCPOLARITY_HIGH);
  LL_TIM_CC_DisableChannel(t, hw->gp_sib_ch);  // sibling drives no pin
  set_pin_af(ch, hw->gp_af);
  if (hw->gp_advanced) {
    LL_TIM_EnableAllOutputs(t);  // BDTR.MOE
  }
  if (!burst) {
    LL_TIM_CC_EnableChannel(t, hw->gp_out_ch);
  }
}

// Configure both timebase and output for an on channel; park it low on a period that doesn't fit.
static bool config_gp_channel(int ch, bool sync, bool burst) {
  uint32_t arr;
  uint64_t tick_ps;
  if (!config_gp_timebase(ch, chan[ch].period_ps, sync, &arr, &tick_ps)) {
    drive_pin_low(ch);
    return false;
  }
  config_gp_output(ch, arr, tick_ps, burst);
  return true;
}

// ---- Burst -----------------------------------------------------------------

// Size the BMC idle window that precedes the ncyc run pulses (see the constant block). Convert the
// absolute-time target to whole periods so the window does not balloon at slow periods; floor it at
// BURST_IDLE_MIN, and never let the whole frame (idle + ncyc) overflow the 16-bit period register.
static uint32_t burst_idle_ticks(uint64_t period_ps, uint32_t ncyc) {
  uint32_t idle = (uint32_t)((BURST_IDLE_TARGET_PS + period_ps - 1) / period_ps);
  if (idle < BURST_IDLE_MIN) {
    idle = BURST_IDLE_MIN;
  }
  if (idle > 65536U - ncyc) {
    idle = 65536U - ncyc;
  }
  return idle;
}

// Pick prescaler + ARR for the repetition timer (TIM5, 32-bit) to count one burst-repeat interval.
// Smallest prescaler that fits the 32-bit counter, for the finest cadence step. rep_ps is already
// validated into [frame + 50 ms, 60 s], so it always fits (div 1 reaches ~34 s, div 2 ~69 s).
static void select_rep_timer(uint64_t rep_ps, uint32_t* out_psc, uint32_t* out_arr) {
  const uint64_t span = GP_TICK_PS * ((uint64_t)GP_ARR32_MAX + 1);  // reach at div 1
  uint64_t div = (rep_ps + span - 1) / span;
  if (div < 1) {
    div = 1;
  }
  uint64_t ticks = rep_ps / (GP_TICK_PS * div);
  if (ticks < 1) {
    ticks = 1;
  }
  *out_psc = (uint32_t)(div - 1);
  *out_arr = (uint32_t)(ticks - 1);
}

// Arm one burst frame: start the HRTIM burst controller (it self-stops at its BMPER interrupt) or
// open the GP software count. Called inline from apply_all for the first frame, and from the
// repetition timer's (TIM5) update ISR for every frame after.
static void burst_arm_frame(void) {
  if (g_hrtim_burst_outputs) {
    // HRTIM regime: start the controller with outputs disabled (the burst begins idle), then
    // enable the outputs while the controller holds them idle.
    LL_HRTIM_DisableOutput(HRTIM1, g_hrtim_burst_outputs);
    LL_HRTIM_ClearFlag_BMPER(HRTIM1);
    LL_HRTIM_BM_Enable(HRTIM1);
    LL_HRTIM_BM_Start(HRTIM1);
    LL_HRTIM_EnableOutput(HRTIM1, g_hrtim_burst_outputs);
  } else if (g_gp_burst_count_ch >= 0) {
    // GP regime: arm the software count. The first update opens the run window (so the first pulse
    // is whole), then ncyc more updates close it. The update IRQ runs below ~2 kHz here, so the
    // count is exact.
    g_gp_burst_remaining = chan[g_gp_burst_count_ch].burst_ncyc;
    g_gp_burst_arming = true;
    TIM_TypeDef* ct = channel_hw[g_gp_burst_count_ch].gp_timer;
    LL_TIM_ClearFlag_UPDATE(ct);
    LL_TIM_EnableIT_UPDATE(ct);
  }
}

// Repetition timer (TIM5) update: one overflow per burst-repeat interval -- re-arm the next frame.
// Hardware-timed off the 125 MHz clock, so the cadence stays phase-coherent with the burst and free
// of the scheduler's jitter. g_burst_active guards a stale tick during reconfiguration.
void TIM5_IRQHandler(void) {
  if (!LL_TIM_IsActiveFlag_UPDATE(TIM5)) {
    return;
  }
  LL_TIM_ClearFlag_UPDATE(TIM5);
  if (g_burst_active) {
    burst_arm_frame();
  }
}

// Enable or disable every gated GP output (the bursting domain) together.
static void gp_burst_set_outputs(uint32_t mask, bool on) {
  for (int c = 0; c < NUM_CHANNELS; c++) {
    if (mask & (1u << c)) {
      const channel_hw_t* hw = &channel_hw[c];
      if (on) {
        LL_TIM_CC_EnableChannel(hw->gp_timer, hw->gp_out_ch);
      } else {
        LL_TIM_CC_DisableChannel(hw->gp_timer, hw->gp_out_ch);
      }
    }
  }
}

// HRTIM burst end (BMPER): the run window has emitted exactly ncyc pulses; park the outputs and
// stop the controller. Routed to the master vector (RM0440 "hrtim_it1").
void HRTIM1_Master_IRQHandler(void) {
  if (LL_HRTIM_IsActiveFlag_BMPER(HRTIM1)) {
    LL_HRTIM_DisableOutput(HRTIM1, g_hrtim_burst_outputs);
    LL_HRTIM_BM_Disable(HRTIM1);
    LL_HRTIM_ClearFlag_BMPER(HRTIM1);
  }
}

static int gp_ch_for_timer(TIM_TypeDef* t) {
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    if (channel_hw[ch].gp_timer == t) {
      return ch;
    }
  }
  return -1;
}

// GP timer update (rollover) handler, told its hardware timer (not a channel index) so it stays
// correct under any channel<->pin remap. Two jobs:
//   1. Per-pulse LED: toggle this timer's channel LED if it's blinking per pulse (slow continuous).
//   2. Burst counting, when this timer is the bursting domain's count timer: the arming update
//      opens the run window on every gated output (a period boundary, so the first pulse is whole);
//      ncyc updates later it closes the window. The repetition timer (TIM5) re-arms the next frame.
// A channel is never both (per-pulse requires not-bursting), so the two paths don't fight over the
// timer's update interrupt.
static void gp_update(TIM_TypeDef* t) {
  if (!LL_TIM_IsActiveFlag_UPDATE(t)) {
    return;
  }
  LL_TIM_ClearFlag_UPDATE(t);

  int ch = gp_ch_for_timer(t);
  if (ch >= 0 && chan[ch].led_per_pulse) {
    chan[ch].led_state = !chan[ch].led_state;
    gpio_set_or_clr(channel_hw[ch].led, chan[ch].led_state);
  }

  if (g_gp_burst_count_ch < 0 || channel_hw[g_gp_burst_count_ch].gp_timer != t) {
    return;
  }
  if (g_gp_burst_arming) {
    gp_burst_set_outputs(g_gp_burst_out_mask, true);
    g_gp_burst_arming = false;
    return;
  }
  if (g_gp_burst_remaining == 0) {
    return;
  }
  if (--g_gp_burst_remaining == 0) {
    gp_burst_set_outputs(g_gp_burst_out_mask, false);
    LL_TIM_DisableIT_UPDATE(t);
  }
}

void TIM2_IRQHandler(void) {
  gp_update(TIM2);
}
void TIM1_UP_TIM16_IRQHandler(void) {
  gp_update(TIM1);
}
void TIM3_IRQHandler(void) {
  gp_update(TIM3);
}
void TIM8_UP_IRQHandler(void) {
  gp_update(TIM8);
}

// Program the HRTIM burst mode controller for the bursting HRTIM-regime domain represented by ch.
static void config_hrtim_burst(int ch, uint32_t outputs) {
  const uint32_t ncyc = chan[ch].burst_ncyc;
  const uint32_t idle = burst_idle_ticks(chan[ch].period_ps, ncyc);

  uint32_t clk_src;
  if (g_mode == MODE_SYNC) {
    clk_src = LL_HRTIM_BM_CLKSRC_MASTER;
  } else {
    switch (channel_hw[ch].hrtim_timer) {
      case LL_HRTIM_TIMER_A:
        clk_src = LL_HRTIM_BM_CLKSRC_TIMER_A;
        break;
      case LL_HRTIM_TIMER_B:
        clk_src = LL_HRTIM_BM_CLKSRC_TIMER_B;
        break;
      case LL_HRTIM_TIMER_E:
        clk_src = LL_HRTIM_BM_CLKSRC_TIMER_E;
        break;
      default:
        clk_src = LL_HRTIM_BM_CLKSRC_TIMER_F;
        break;
    }
  }

  LL_HRTIM_BM_SetMode(HRTIM1, LL_HRTIM_BM_MODE_CONTINOUS);
  LL_HRTIM_BM_SetClockSrc(HRTIM1, clk_src);
  LL_HRTIM_BM_SetPrescaler(HRTIM1, LL_HRTIM_BM_PRESCALER_DIV1);
  LL_HRTIM_BM_DisablePreload(HRTIM1);
  LL_HRTIM_BM_SetCompare(HRTIM1, idle - 1);
  LL_HRTIM_BM_SetPeriod(HRTIM1, idle + ncyc - 1);

  g_hrtim_burst_outputs = outputs;
  LL_HRTIM_EnableIT_BMPER(HRTIM1);
}

// ---- Validation ------------------------------------------------------------

static int validate(int ch) {
  if (!chan[ch].on) {
    return 0;
  }
  const uint64_t T = chan[ch].period_ps;
  const uint64_t W = chan[ch].width_ps;
  const uint64_t D = chan[ch].delay_ps;
  if (!hrtim_dll_ready) {
    return -3;
  }
  if (T == 0 || W == 0) {
    return -1;
  }
  if (W >= T) {
    return -2;
  }
  if (D + W > T) {
    return -5;
  }

  if (uses_hrtim(ch)) {
    uint8_t ck;
    uint32_t per;
    if (!select_ckpsc(T, &ck, &per)) {
      return -4;
    }
  } else {
    uint32_t psc, arr;
    uint64_t tick;
    if (!select_gp_psc(ch, T, &psc, &arr, &tick)) {
      return -4;
    }
  }

  if (chan[ch].burst_on) {
    const uint32_t n = chan[ch].burst_ncyc;
    if (n == 0 || n > BURST_NCYC_MAX) {
      return -6;
    }
    // One HRTIM burst controller and one GP burst counter -> at most one bursting domain.
    for (int i = 0; i < NUM_CHANNELS; i++) {
      if (!same_domain(ch, i) && chan[i].on && chan[i].burst_on) {
        return -7;
      }
    }
    if (chan[ch].burst_rep_ps > BURST_REP_MAX_PS) {
      return -9;
    }
    // The repetition must outlast one whole burst frame plus a guard for ISR latency. The HRTIM
    // frame is the BMC idle window ahead of the ncyc pulses; the GP frame is the ncyc pulses plus
    // the one-period arming gap.
    uint64_t frame_ps =
        uses_hrtim(ch) ? (uint64_t)(burst_idle_ticks(T, n) + n) * T : (uint64_t)(n + 1) * T;
    if (chan[ch].burst_rep_ps <= frame_ps + BURST_REP_MARGIN_PS) {
      return -8;
    }
  }
  return 0;
}

// ---- apply_all -------------------------------------------------------------

// Tear down and rebuild the entire timer configuration from the current state.
static void apply_all(void) {
  // Quiesce burst first: stop the repetition timer so no stale tick re-arms mid-rebuild.
  g_burst_active = false;
  LL_TIM_DisableIT_UPDATE(TIM5);
  LL_TIM_DisableCounter(TIM5);
  LL_TIM_ClearFlag_UPDATE(TIM5);
  g_hrtim_burst_outputs = 0;
  g_gp_burst_count_ch = -1;
  g_gp_burst_out_mask = 0;
  g_gp_burst_arming = false;
  LL_HRTIM_DisableIT_BMPER(HRTIM1);
  LL_HRTIM_BM_Disable(HRTIM1);
  LL_HRTIM_ClearFlag_BMPER(HRTIM1);

  // Stop every timer and disable every output.
  LL_HRTIM_TIM_CounterDisable(HRTIM1, LL_HRTIM_TIMER_MASTER | LL_HRTIM_TIMER_A | LL_HRTIM_TIMER_B |
                                          LL_HRTIM_TIMER_E | LL_HRTIM_TIMER_F);
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    LL_HRTIM_DisableOutput(HRTIM1, channel_hw[ch].hrtim_output);
    TIM_TypeDef* t = channel_hw[ch].gp_timer;
    LL_TIM_DisableIT_UPDATE(t);
    LL_TIM_DisableCounter(t);
    LL_TIM_CC_DisableChannel(t, channel_hw[ch].gp_out_ch);
  }

  if (!hrtim_dll_ready) {
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
      drive_pin_low(ch);
    }
    return;
  }

  // In sync mode every channel shares channel 0's period, so the whole device is in one regime.
  const bool sync = (g_mode == MODE_SYNC);
  const bool sync_hrtim = sync && uses_hrtim(0);
  const bool sync_gp = sync && !uses_hrtim(0);

  uint32_t hrtim_run_mask = 0;  // HRTIM timers to enable together
  uint32_t gp_run_mask = 0;     // bitmask of channels whose GP timer should be enabled
  uint32_t hrtim_burst_outs = 0;
  int burst_ch = -1;
  bool burst_is_gp = false;

  if (sync_hrtim) {
    // The HRTIM master defines the period; it must run continuously so its period event (the slave
    // reset and the burst clock) keeps firing.
    uint8_t ck;
    uint32_t per;
    if (select_ckpsc(chan[0].period_ps, &ck, &per)) {
      LL_HRTIM_TIM_SetPrescaler(HRTIM1, LL_HRTIM_TIMER_MASTER, hrtim_ckpsc_to_ll[ck]);
      LL_HRTIM_TIM_SetCounterMode(HRTIM1, LL_HRTIM_TIMER_MASTER, LL_HRTIM_MODE_CONTINUOUS);
      LL_HRTIM_TIM_SetPeriod(HRTIM1, LL_HRTIM_TIMER_MASTER, per);
      LL_HRTIM_TIM_SetRepetition(HRTIM1, LL_HRTIM_TIMER_MASTER, 0);
      hrtim_run_mask = LL_HRTIM_TIMER_MASTER;
    }
  } else if (sync_gp) {
    // The TIM1-owning channel is the GP-regime sync master: it must run with the shared period and
    // drive the slaves' ITR0 even when its own output is off. (Its output is configured separately,
    // in the per-channel loop, only if that channel is on.)
    const int mch = gp_master_ch();
    uint32_t arr;
    uint64_t tick;
    if (mch >= 0 && config_gp_timebase(mch, chan[0].period_ps, /*sync=*/true, &arr, &tick)) {
      gp_run_mask |= (1u << mch);
    }
  }

  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    if (!chan[ch].on || validate(ch) != 0) {
      drive_pin_low(ch);
      continue;
    }
    const bool burst = chan[ch].burst_on;
    if (uses_hrtim(ch)) {
      uint8_t ck;
      uint32_t per;
      if (!select_ckpsc(chan[ch].period_ps, &ck, &per)) {
        drive_pin_low(ch);
        continue;
      }
      config_hrtim_channel(ch, ck, per, sync_hrtim, burst);
      hrtim_run_mask |= channel_hw[ch].hrtim_timer;
      if (burst) {
        hrtim_burst_outs |= channel_hw[ch].hrtim_output;
        if (burst_ch < 0) {
          burst_ch = ch;
          burst_is_gp = false;
        }
      }
    } else {
      config_gp_channel(ch, sync, burst);
      gp_run_mask |= (1u << ch);
      if (burst) {
        if (burst_ch < 0) {
          burst_ch = ch;
          burst_is_gp = true;
        }
      }
    }
  }

  // Enable the HRTIM timers phase-aligned (reset counters to 0 first, so a sync start coincides).
  if (hrtim_run_mask) {
    LL_HRTIM_CounterReset(HRTIM1, hrtim_run_mask);
    LL_HRTIM_TIM_CounterEnable(HRTIM1, hrtim_run_mask);
  }

  // Enable the GP timers. In sync, TIM1 (master) is enabled last and resets the slaves; preset all
  // counters to 0 so the first shared period is aligned.
  if (gp_run_mask) {
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
      if (gp_run_mask & (1u << ch)) {
        LL_TIM_SetCounter(channel_hw[ch].gp_timer, 0);
      }
    }
    // Enable slaves first (they wait for the master's trigger in sync), then TIM1.
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
      if ((gp_run_mask & (1u << ch)) && channel_hw[ch].gp_timer != TIM1) {
        LL_TIM_EnableCounter(channel_hw[ch].gp_timer);
      }
    }
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
      if ((gp_run_mask & (1u << ch)) && channel_hw[ch].gp_timer == TIM1) {
        LL_TIM_EnableCounter(TIM1);
      }
    }
  }

  // Arm burst, if any (the bursting domain's counter is now running).
  if (burst_ch >= 0) {
    g_burst_active = true;
    if (burst_is_gp) {
      // Gate every on GP output in the bursting domain; count the domain's timebase -- the TIM1
      // master channel in sync (it always runs there), the single channel itself in async.
      g_gp_burst_out_mask = 0;
      for (int c = 0; c < NUM_CHANNELS; c++) {
        if (chan[c].on && (gp_run_mask & (1u << c)) && same_domain(burst_ch, c)) {
          g_gp_burst_out_mask |= (1u << c);
        }
      }
      g_gp_burst_count_ch = sync ? gp_master_ch() : burst_ch;
    } else {
      config_hrtim_burst(burst_ch, hrtim_burst_outs);
    }
    // Fire the first frame now, then let the repetition timer (TIM5) re-arm each subsequent frame
    // at the programmed interval -- hardware-timed and phase-coherent, no scheduler in the path.
    uint32_t rep_psc, rep_arr;
    select_rep_timer(chan[burst_ch].burst_rep_ps, &rep_psc, &rep_arr);
    LL_TIM_SetCounterMode(TIM5, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetPrescaler(TIM5, rep_psc);
    LL_TIM_SetAutoReload(TIM5, rep_arr);
    LL_TIM_DisableARRPreload(TIM5);
    LL_TIM_SetCounter(TIM5, 0);
    burst_arm_frame();
    LL_TIM_ClearFlag_UPDATE(TIM5);
    LL_TIM_EnableIT_UPDATE(TIM5);
    LL_TIM_EnableCounter(TIM5);
  }

  // LED policy: a slow continuous channel (freq <= 2 Hz, always GP regime, not bursting) toggles
  // its LED once per pulse off its GP timer's update (rollover) interrupt -- one toggle per period
  // (in sync, the slave's reset by the master generates the update). Everything else (fast,
  // bursting, off) is the heartbeat in update_leds. Recomputed for every channel each apply_all.
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    const bool per_pulse = chan[ch].on && !chan[ch].burst_on && validate(ch) == 0 &&
                           chan[ch].period_ps >= LED_SLOW_PERIOD_PS;
    chan[ch].led_per_pulse = per_pulse;
    if (per_pulse) {
      chan[ch].led_state = false;
      gpio_clr(channel_hw[ch].led);
      LL_TIM_ClearFlag_UPDATE(channel_hw[ch].gp_timer);
      LL_TIM_EnableIT_UPDATE(channel_hw[ch].gp_timer);
    }
  }
}

// ---- SCPI hooks (called from scpi.c) ---------------------------------------

static const char* err_for_rc(int rc) {
  switch (rc) {
    case 0:
      return NULL;
    case -1:
      return "-200,\"Period and width must be > 0\"";
    case -2:
      return "-200,\"Width must be < period\"";
    case -3:
      return "-200,\"HRTIM DLL not ready\"";
    case -4:
      return "-200,\"Period out of range (24 ns .. 34 s)\"";
    case -5:
      return "-200,\"Delay + width must be <= period\"";
    case -6:
      return "-200,\"Burst count out of range (1 .. 65534)\"";
    case -7:
      return "-200,\"Only one domain can burst at a time\"";
    case -8:
      return "-200,\"Burst interval too short (must exceed one burst frame)\"";
    case -9:
      return "-200,\"Burst interval out of range (max 60 s)\"";
    default:
      return "-200,\"Apply failed\"";
  }
}

static void set_domain_period(int ch, uint64_t ps) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (same_domain(ch, i)) {
      chan[i].period_ps = ps;
    }
  }
}

const char* pulsegen_set_state(int ch, bool on) {
  chan[ch].on = on;
  apply_all();
  return err_for_rc(validate(ch));
}

const char* pulsegen_set_period_ps(int ch, uint64_t ps) {
  set_domain_period(ch, ps);
  apply_all();
  return err_for_rc(validate(ch));
}

const char* pulsegen_set_width_ps(int ch, uint64_t ps) {
  chan[ch].width_ps = ps;
  apply_all();
  return err_for_rc(validate(ch));
}

const char* pulsegen_set_delay_ps(int ch, uint64_t ps) {
  chan[ch].delay_ps = ps;
  apply_all();
  return err_for_rc(validate(ch));
}

const char* pulsegen_set_burst_state(int ch, bool on) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (same_domain(ch, i)) {
      // Disabling a live burst also disables the domain's outputs: the domain period IS the
      // intra-burst spacing, so "continuous after burst off" would stream at the burst's
      // instantaneous rate. Fail safe; the user re-enables outputs explicitly.
      if (!on && chan[i].burst_on) {
        chan[i].on = false;
      }
      chan[i].burst_on = on;
    }
  }
  apply_all();
  return err_for_rc(validate(ch));
}

const char* pulsegen_set_burst_ncyc(int ch, uint32_t ncyc) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (same_domain(ch, i)) {
      chan[i].burst_ncyc = ncyc;
    }
  }
  apply_all();
  return err_for_rc(validate(ch));
}

const char* pulsegen_set_burst_period_ps(int ch, uint64_t ps) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (same_domain(ch, i)) {
      chan[i].burst_rep_ps = ps;
    }
  }
  apply_all();
  return err_for_rc(validate(ch));
}

bool pulsegen_get_burst_state(int ch) {
  return chan[ch].burst_on;
}
uint32_t pulsegen_get_burst_ncyc(int ch) {
  return chan[ch].burst_ncyc;
}
uint64_t pulsegen_get_burst_period_ps(int ch) {
  return chan[ch].burst_rep_ps;
}

const char* pulsegen_set_mode(bool sync) {
  g_mode = sync ? MODE_SYNC : MODE_ASYNC;
  if (sync) {
    for (int i = 1; i < NUM_CHANNELS; i++) {
      chan[i].period_ps = chan[0].period_ps;
      chan[i].burst_on = chan[0].burst_on;
      chan[i].burst_ncyc = chan[0].burst_ncyc;
      chan[i].burst_rep_ps = chan[0].burst_rep_ps;
    }
  }
  apply_all();
  return NULL;
}

const char* pulsegen_mode_str(void) {
  return (g_mode == MODE_SYNC) ? "SYNC" : "ASYNC";
}

void pulsegen_reset_all(void) {
  g_mode = MODE_ASYNC;
  for (int i = 0; i < NUM_CHANNELS; i++) {
    chan[i].period_ps = 0;
    chan[i].width_ps = 0;
    chan[i].delay_ps = 0;
    chan[i].on = false;
    chan[i].burst_on = false;
    chan[i].burst_ncyc = 1;
    chan[i].burst_rep_ps = BURST_REP_DEFAULT_PS;
  }
  apply_all();
}

// ---- LEDs ------------------------------------------------------------------

static void update_leds(void) {
  static bool on_phase = false;
  on_phase = !on_phase;

  if (g_rulos_hse_failed) {
    gpio_clr(LED_CLOCK);
  } else {
    gpio_set_or_clr(LED_CLOCK, on_phase);
  }
  for (int i = 0; i < NUM_CHANNELS; i++) {
    // Per-pulse (slow continuous) channels are driven by the GP update ISR; the heartbeat owns the
    // fast and bursting channels and clears the off ones.
    if (chan[i].led_per_pulse) {
      continue;
    }
    gpio_set_or_clr(channel_hw[i].led, chan[i].on && on_phase);
  }
  bool usb_blink_off = !on_phase && scpi_consume_recent_activity();
  gpio_set_or_clr(LED_USB, scpi_usb_ready() && !usb_blink_off);
}

// ---- Periodic task ---------------------------------------------------------

static void periodic_task(void* data) {
  schedule_us(100000, periodic_task, NULL);

  if (g_rulos_hse_failed) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
      chan[i].on = false;
    }
    apply_all();
    static bool on = false;
    on = !on;
    gpio_clr(LED_CLOCK);
    for (int i = 0; i < NUM_CHANNELS; i++) {
      gpio_set_or_clr(channel_hw[i].led, on);
    }
    return;
  }
  update_leds();
}

// ---- Init ------------------------------------------------------------------

static void init_hrtim(void) {
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_HRTIM1);
  LL_HRTIM_ConfigDLLCalibration(HRTIM1, LL_HRTIM_DLLCALIBRATION_MODE_CONTINUOUS,
                                LL_HRTIM_DLLCALIBRATION_RATE_3);
  for (uint32_t spin = 0; spin < 100000; spin++) {
    if (LL_HRTIM_IsActiveFlag_DLLRDY(HRTIM1)) {
      hrtim_dll_ready = true;
      break;
    }
  }
  // HRTIM burst-end interrupt (deadline is a whole idle window, relaxed priority).
  HAL_NVIC_SetPriority(HRTIM1_Master_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(HRTIM1_Master_IRQn);
}

static void init_gp_timers(void) {
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM8);
  // TIM5: the burst-repetition timer (32-bit, no output pins -- counter + update IRQ only).
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM5);

  // Burst interrupts (the GP per-channel update counts plus the TIM5 repetition tick) are enabled
  // per-burst; arm the NVIC lines once, at the same relaxed priority as the HRTIM burst-end IRQ.
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    HAL_NVIC_SetPriority(channel_hw[ch].gp_up_irqn, 6, 0);
    HAL_NVIC_EnableIRQ(channel_hw[ch].gp_up_irqn);
  }
  HAL_NVIC_SetPriority(TIM5_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(TIM5_IRQn);
}

static void init_gpio(void) {
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);

  for (int i = 0; i < NUM_CHANNELS; i++) {
    drive_pin_low(i);
  }

  gpio_make_output(LED_CHAN0);
  gpio_make_output(LED_CHAN1);
  gpio_make_output(LED_CHAN2);
  gpio_make_output(LED_CHAN3);
  gpio_make_output(LED_CLOCK);
  gpio_make_output(LED_USB);
}

int main(void) {
  rulos_hal_init();
  init_clock(10000, TIMER1);

  // Debug UART: USART1 on PB6/PB7 (RULOS_UART0_*_PIN overridden in SConstruct), off the PA9/PA10
  // output pins.
  uart_init(&uart, /*uart_id=*/0, 1000000);
  log_bind_uart(&uart);

  init_gpio();
  pulsegen_scpi_init();
  init_hrtim();
  init_gp_timers();

  pulsegen_reset_all();

  schedule_us(1, periodic_task, NULL);
  scheduler_run();
}
