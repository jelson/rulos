/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * pulsegen: a 4-channel pulse generator on the STM32G474, built around the
 * High-Resolution Timer (HRTIM). Configuration is delivered over USB CDC
 * using a small subset of SCPI; a UART debug port stays available for logs.
 *
 * Clock plan (set in SConstruct via PLLM/PLLN):
 *   HSE 10 MHz / PLLM=1 * PLLN=25 / PLLR=2 = 125 MHz SYSCLK
 *   fHRTIM = 125 MHz; with the HRTIM DLL at 32x oversampling, fHRCK = 4 GHz
 *   and the finest tick is 250 ps.
 *
 * Output pins, and the hardware constraint that shapes the whole design:
 *
 *   Channel 0 -> PC6   HRTIM1_CHF1  (Timer F, output 1)   AF13
 *   Channel 1 -> PC7   HRTIM1_CHF2  (Timer F, output 2)   AF13
 *   Channel 2 -> PC8   HRTIM1_CHE1  (Timer E, output 1)   AF3
 *   Channel 3 -> PC9   HRTIM1_CHE2  (Timer E, output 2)   AF3
 *
 * Note the alternate function differs by pin: the Timer F outputs on PC6/PC7
 * are AF13, but the Timer E outputs on PC8/PC9 are AF3 (per DS12288). The AF
 * is therefore a per-channel property, not a constant.
 *
 * Only HRTIM Timers E and F reach these four pins. A single HRTIM timer has
 * one counter and one period, shared by its two outputs. So channels 0 and 1
 * are forced to a common period (Timer F's), and channels 2 and 3 to another
 * (Timer E's). Four fully-independent periods are physically impossible on
 * this pin set -- that needs a board with four separate HRTIM timers routed
 * out. What IS possible:
 *
 *   ASYNC mode (default): two independent frequency groups.
 *     Group 0 = Timer F = channels 0,1 -- shared period; each channel has its
 *               own rising-edge delay and width (250 ps grid).
 *     Group 1 = Timer E = channels 2,3 -- a second, independent period; again
 *               per-channel delay and width.
 *
 *   SYNC mode: all four channels share one period. The HRTIM master timer
 *     resets both slave timers every period, phase-locking E to F, so a
 *     delay of N ticks on any channel lands at the same real time relative to
 *     a common t=0. This is the mode for phased clocks and for sweeping the
 *     inter-channel rising-edge offset in 250 ps steps.
 *
 * Burst mode: rather than a continuous train, a domain can emit exactly N
 * pulses, rest low, and repeat at a programmable interval (modeled on the
 * Rigol DG1022Z burst feature). The HRTIM burst mode controller (BMC) gates
 * the outputs: it is clocked by the bursting domain's own period roll-over
 * (the master's in SYNC mode), so its idle and run windows are exact integer
 * counts of pulse periods aligned to period boundaries -- N is hardware-
 * exact, no pulse is chopped. Each repetition is armed by a scheduler task
 * and terminated by the burst-period interrupt, so the repetition interval
 * is software-scheduled (~10 ms granularity, one scheduler jiffy) while the
 * count and spacing within a burst stay hardware-exact. There is a single
 * BMC, so in ASYNC mode only one group may burst at a time; SYNC mode bursts
 * all four channels coherently off the master.
 *
 * In both modes, each channel's rising edge is placed by a compare (slot 0 ->
 * CMP1, slot 1 -> CMP3; or the period event for ~0 delay) and its falling
 * edge by the next compare (CMP2 / CMP4). Two channels x (rise, fall) = the
 * timer's four compares, exactly.
 *
 * Frequency range: HRTIM only. ~24 ns minimum period (RM0440 minimum-PER
 * constraint, ~41 MHz) to ~524 us (CKPSC=5, ~1.9 kHz). There is no long-
 * period fallback: TIM3 has a single counter shared across all four channels
 * and can't place sub-ns delays, so it doesn't fit this model. Sub-2 kHz
 * support is a future addition (HRTIM repetition counter, or a separate
 * coarse mode).
 *
 * SCPI command surface:
 *   *IDN?
 *   *RST
 *   MODE ASYNC|SYNC                         (MODE? to query)
 *   OUTPut<n>:STATe ON|OFF
 *   SOURce<n>:PULSe:PERiod  <seconds>       group period (async) / global (sync)
 *   SOURce<n>:PULSe:WIDTh   <seconds>       per channel
 *   SOURce<n>:PULSe:DELay   <seconds>       per-channel rising-edge offset
 *   SOURce<n>:BURSt:STATe ON|OFF            burst on the channel's domain
 *   SOURce<n>:BURSt:NCYCles <count>         pulses per burst (1..63488)
 *   SOURce<n>:BURSt:INTernal:PERiod <sec>   burst repetition interval
 *   SYSTem:ERRor?
 *
 * The three BURSt commands also have query forms (append '?').
 *
 * Reconfiguration tears down and rebuilds the whole HRTIM state on any change,
 * so changing one channel briefly glitches the others -- fine for a bench
 * source that is set up and then observed.
 *
 * Firmware update over USB DFU (no SWD probe needed):
 *
 *   sudo dfu-util -d 1209:71c5,0483:df11 -a 0 -s 0x08000000:leave -D <fw>.bin
 *
 * 1209:71c5 is the runtime VID:PID, 0483:df11 the ST ROM bootloader; the
 * two-tuple lets dfu-util find the running device, auto-detach it into the
 * bootloader, flash, and restart (:leave). If interrupted it stays in the
 * bootloader and the same command retries. Verify with `pgctl.py idn`.
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

#ifndef RULOS_USE_HSE
#error "pulsegen requires RULOS_USE_HSE"
#endif

// NUM_CHANNELS comes from pulsegen.h.
// fHRCK at CKPSC=0 = 32 * 125 MHz = 4 GHz -> 250 ps/tick
#define HRTIM_FINEST_TICK_PS 250ULL

// HRTIM 16-bit period max in high-res mode (RM0440 27.3.4): the period must
// be at least 3 (or higher in mul* modes) and at most 0xFFDF.
#define HRTIM_PER_MAX 0xFFDFU

// Burst mode limits. Each burst frame is idle_ticks + ncyc pulse periods,
// counted by the BMC's 16-bit period register (BMPER = idle + ncyc - 1), so
// idle <= 65536 - ncyc. The idle window is the deadline for the arm sequence
// and the burst-end ISR, so it must stay generous: at least BURST_IDLE_MIN
// periods (~49 us at the 24 ns minimum period), which caps ncyc at
// 65536 - BURST_IDLE_MIN = 63488.
#define BURST_IDLE_MIN 2048U
#define BURST_IDLE_MAX 8192U
#define BURST_NCYC_MAX (65536U - BURST_IDLE_MIN)

#define PS_PER_SEC           1000000000000ULL
#define BURST_REP_DEFAULT_PS PS_PER_SEC
// The repetition interval must cover the whole burst frame plus margin for
// the software re-arm (scheduler jiffy is 10 ms).
#define BURST_REP_MARGIN_PS (50ULL * PS_PER_SEC / 1000)  // 50 ms
// Scheduler Time is 32-bit microseconds with wraparound comparison, valid
// only for offsets below 2^31 us (~35 min); 60 s keeps far inside that.
#define BURST_REP_MAX_PS (60ULL * PS_PER_SEC)

// LEDs.
#define LED_CHAN0 GPIO_C0  // channel 0 activity (output transitioning)
#define LED_CHAN1 GPIO_A3  // channel 1 activity
#define LED_CHAN2 GPIO_A4  // channel 2 activity
#define LED_CHAN3 GPIO_A8  // channel 3 activity
#define LED_CLOCK GPIO_B4  // 10 MHz HSE health
#define LED_USB   GPIO_A5  // USB activity

// ---- Channel hardware mapping ----------------------------------------------

// Channels pair up onto HRTIM timers: group = ch / 2 (group 0 = Timer F =
// ch0,1; group 1 = Timer E = ch2,3). slot = ch & 1 selects which of the two
// outputs and which compare pair the channel owns within its timer:
//   slot 0 -> output 1, rising on CMP1, falling on CMP2
//   slot 1 -> output 2, rising on CMP3, falling on CMP4
typedef struct {
  uint32_t ll_pin;        // LL_GPIO_PIN_n (all on GPIOC)
  uint8_t af;             // GPIO alternate function: TF outputs AF13, TE AF3
  uint32_t hrtim_timer;   // LL_HRTIM_TIMER_E / F
  uint32_t hrtim_output;  // LL_HRTIM_OUTPUT_TE1/TE2 or TF1/TF2
  gpio_pin_t led;
} channel_hw_t;

static const channel_hw_t channel_hw[NUM_CHANNELS] = {
    [0] = {.ll_pin = LL_GPIO_PIN_6,
           .af = 13,
           .hrtim_timer = LL_HRTIM_TIMER_F,
           .hrtim_output = LL_HRTIM_OUTPUT_TF1,
           .led = LED_CHAN0},
    [1] = {.ll_pin = LL_GPIO_PIN_7,
           .af = 13,
           .hrtim_timer = LL_HRTIM_TIMER_F,
           .hrtim_output = LL_HRTIM_OUTPUT_TF2,
           .led = LED_CHAN1},
    [2] = {.ll_pin = LL_GPIO_PIN_8,
           .af = 3,
           .hrtim_timer = LL_HRTIM_TIMER_E,
           .hrtim_output = LL_HRTIM_OUTPUT_TE1,
           .led = LED_CHAN2},
    [3] = {.ll_pin = LL_GPIO_PIN_9,
           .af = 3,
           .hrtim_timer = LL_HRTIM_TIMER_E,
           .hrtim_output = LL_HRTIM_OUTPUT_TE2,
           .led = LED_CHAN3},
};

// group 0 = Timer F (ch0,1); group 1 = Timer E (ch2,3).
static const uint32_t group_timer[2] = {LL_HRTIM_TIMER_F, LL_HRTIM_TIMER_E};

// ---- Channel runtime state -------------------------------------------------

typedef enum {
  MODE_ASYNC = 0,  // two independent frequency groups (default)
  MODE_SYNC,       // all channels share one period, phase-locked via master
} pulsegen_mode_t;

// Per-channel user parameters, in picoseconds (uint64_t has ample headroom;
// 1 s = 1e12 ps). period_ps is kept equal across a channel's domain -- the
// group in async, all four in sync -- so it always reads back the effective
// period for that channel.
static uint64_t chan_period_ps[NUM_CHANNELS];
static uint64_t chan_width_ps[NUM_CHANNELS];
static uint64_t chan_delay_ps[NUM_CHANNELS];  // rising-edge offset
static bool chan_on[NUM_CHANNELS];

// Burst parameters; like period, kept equal across the channel's domain so
// each channel always reads back its effective setting.
static bool chan_burst_on[NUM_CHANNELS];
static uint32_t chan_burst_ncyc[NUM_CHANNELS];
static uint64_t chan_burst_rep_ps[NUM_CHANNELS];

static pulsegen_mode_t g_mode = MODE_ASYNC;

// True iff channels a and b share a domain (and therefore period and burst
// settings): the same HRTIM timer in async mode, the whole device in sync.
static bool same_domain(int a, int b) {
  return g_mode == MODE_SYNC || (a / 2) == (b / 2);
}

// ---- Globals ---------------------------------------------------------------

static UartState_t uart;
static bool hrtim_dll_ready = false;

// Active burst hardware state, owned by apply_all().
static uint32_t g_burst_outputs;  // OENR/ODISR mask; 0 = no active burst
static Time g_burst_rep_us;       // repetition interval of the active config
static bool g_burst_task_live;    // a burst_rep_task sits in the scheduler

// ---- HRTIM helpers ---------------------------------------------------------

static const uint32_t hrtim_ckpsc_to_ll[6] = {
    LL_HRTIM_PRESCALERRATIO_MUL32, LL_HRTIM_PRESCALERRATIO_MUL16, LL_HRTIM_PRESCALERRATIO_MUL8,
    LL_HRTIM_PRESCALERRATIO_MUL4,  LL_HRTIM_PRESCALERRATIO_MUL2,  LL_HRTIM_PRESCALERRATIO_DIV1,
};

// Per-CKPSC minimum PER value (RM0440 Table 218, "Minimum PER value"). Also
// used as the minimum usable compare value: a rising-edge delay below this
// many ticks is treated as zero (set on the period event instead).
static const uint16_t hrtim_min_per[6] = {
    0x0060, 0x0030, 0x0018, 0x000C, 0x0006, 0x0003,
};

// Pick the finest CKPSC whose period (in that CKPSC's ticks) fits the HRTIM
// 16-bit period register. Returns false if the period is too short for even
// the coarsest prescaler's minimum, or too long for the finest. out_ckpsc and
// out_per (the PER register value, = period in ticks) are filled on success.
static bool select_ckpsc(uint64_t period_ps, uint8_t* out_ckpsc, uint32_t* out_per) {
  for (uint8_t k = 0; k <= 5; k++) {
    uint64_t tick_ps = HRTIM_FINEST_TICK_PS << k;
    uint64_t per = period_ps / tick_ps;
    if (per > HRTIM_PER_MAX)
      continue;
    if (per < hrtim_min_per[k])
      continue;
    *out_ckpsc = k;
    *out_per = (uint32_t)per;
    return true;
  }
  return false;
}

static void set_pin_af(int ch) {
  // PC6/PC7 are in the low half (pins 0..7); PC8/PC9 are in the high half
  // (pins 8..15). LL_GPIO has separate AF setters for each half. The AF
  // number is per channel (Timer F outputs AF13, Timer E AF3).
  const uint32_t pin = channel_hw[ch].ll_pin;
  const uint8_t af = channel_hw[ch].af;
  if (pin <= LL_GPIO_PIN_7) {
    LL_GPIO_SetAFPin_0_7(GPIOC, pin, af);
  } else {
    LL_GPIO_SetAFPin_8_15(GPIOC, pin, af);
  }
  LL_GPIO_SetPinMode(GPIOC, pin, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetPinSpeed(GPIOC, pin, LL_GPIO_SPEED_FREQ_VERY_HIGH);
  LL_GPIO_SetPinOutputType(GPIOC, pin, LL_GPIO_OUTPUT_PUSHPULL);
  LL_GPIO_SetPinPull(GPIOC, pin, LL_GPIO_PULL_NO);
}

static void drive_pin_low(int ch) {
  const uint32_t pin = channel_hw[ch].ll_pin;
  LL_GPIO_ResetOutputPin(GPIOC, pin);
  LL_GPIO_SetPinMode(GPIOC, pin, LL_GPIO_MODE_OUTPUT);
  LL_GPIO_SetPinOutputType(GPIOC, pin, LL_GPIO_OUTPUT_PUSHPULL);
}

// Configure one channel's HRTIM output within its (already period/prescaler-
// configured) timer: place the rising edge via the channel's "rising" compare
// (or the period event for ~0 delay) and the falling edge via its "falling"
// compare, then enable the output and switch the pin to AF13. tick_ps and
// per_ticks describe the timer the channel belongs to. A bursting channel is
// gated by the burst mode controller and left disabled here; the repetition
// task enables it for the duration of each burst.
static void config_channel_output(int ch, uint8_t ckpsc, uint32_t per_ticks, bool burst) {
  const channel_hw_t* hw = &channel_hw[ch];
  const uint64_t tick_ps = HRTIM_FINEST_TICK_PS << ckpsc;
  const uint32_t min_t = hrtim_min_per[ckpsc];

  uint32_t delay_t = (uint32_t)(chan_delay_ps[ch] / tick_ps);
  // A bursting output must not use the set-on-period path: a set event
  // coincident with the roll-over that opens the run window races the burst
  // controller's idle release. Clamp the rising edge to the first usable
  // compare value instead.
  if (burst && delay_t < min_t)
    delay_t = min_t;
  uint32_t width_t = (uint32_t)(chan_width_ps[ch] / tick_ps);
  if (width_t < min_t)
    width_t = min_t;
  uint32_t fall_t = delay_t + width_t;
  if (fall_t > per_ticks - 1)
    fall_t = per_ticks - 1;
  const bool zero_delay = (delay_t < min_t);

  // slot 0 owns CMP1 (rise) / CMP2 (fall); slot 1 owns CMP3 / CMP4.
  uint32_t set_src, reset_src;
  if ((ch & 1) == 0) {
    if (!zero_delay)
      LL_HRTIM_TIM_SetCompare1(HRTIM1, hw->hrtim_timer, delay_t);
    LL_HRTIM_TIM_SetCompare2(HRTIM1, hw->hrtim_timer, fall_t);
    set_src = zero_delay ? LL_HRTIM_OUTPUTSET_TIMPER : LL_HRTIM_OUTPUTSET_TIMCMP1;
    reset_src = LL_HRTIM_OUTPUTRESET_TIMCMP2;
  } else {
    if (!zero_delay)
      LL_HRTIM_TIM_SetCompare3(HRTIM1, hw->hrtim_timer, delay_t);
    LL_HRTIM_TIM_SetCompare4(HRTIM1, hw->hrtim_timer, fall_t);
    set_src = zero_delay ? LL_HRTIM_OUTPUTSET_TIMPER : LL_HRTIM_OUTPUTSET_TIMCMP3;
    reset_src = LL_HRTIM_OUTPUTRESET_TIMCMP4;
  }

  LL_HRTIM_OUT_SetPolarity(HRTIM1, hw->hrtim_output, LL_HRTIM_OUT_POSITIVE_POLARITY);
  LL_HRTIM_OUT_SetOutputSetSrc(HRTIM1, hw->hrtim_output, set_src);
  LL_HRTIM_OUT_SetOutputResetSrc(HRTIM1, hw->hrtim_output, reset_src);
  // IDLEM selects which outputs the burst mode controller gates; outputs
  // with IDLEM clear run continuously, undisturbed by another group's burst.
  LL_HRTIM_OUT_SetIdleMode(HRTIM1, hw->hrtim_output,
                           burst ? LL_HRTIM_OUT_IDLE_WHEN_BURST : LL_HRTIM_OUT_NO_IDLE);
  LL_HRTIM_OUT_SetIdleLevel(HRTIM1, hw->hrtim_output, LL_HRTIM_OUT_IDLELEVEL_INACTIVE);
  LL_HRTIM_OUT_SetFaultState(HRTIM1, hw->hrtim_output, LL_HRTIM_OUT_FAULTSTATE_NO_ACTION);
  LL_HRTIM_OUT_SetChopperMode(HRTIM1, hw->hrtim_output, LL_HRTIM_OUT_CHOPPERMODE_DISABLED);

  set_pin_af(ch);  // route the pin to its HRTIM output AF
  if (!burst)
    LL_HRTIM_EnableOutput(HRTIM1, hw->hrtim_output);
}

// Set up a slave timer's counter (prescaler, period, counting mode) and its
// reset source. sync => reset on the master period so this timer phase-locks
// to the other group; async => free-running.
static void config_group_timer(int g, uint8_t ckpsc, uint32_t per_ticks, bool sync) {
  const uint32_t timer = group_timer[g];
  LL_HRTIM_TIM_SetPrescaler(HRTIM1, timer, hrtim_ckpsc_to_ll[ckpsc]);
  LL_HRTIM_TIM_SetCounterMode(HRTIM1, timer, LL_HRTIM_MODE_CONTINUOUS);
  LL_HRTIM_TIM_SetPeriod(HRTIM1, timer, per_ticks);
  LL_HRTIM_TIM_SetRepetition(HRTIM1, timer, 0);
  LL_HRTIM_TIM_SetCountingMode(HRTIM1, timer, LL_HRTIM_COUNTING_MODE_UP);
  LL_HRTIM_TIM_DisablePreload(HRTIM1, timer);
  LL_HRTIM_TIM_SetResetTrig(HRTIM1, timer,
                            sync ? LL_HRTIM_RESETTRIG_MASTER_PER : LL_HRTIM_RESETTRIG_NONE);
}

// ---- Burst mode ------------------------------------------------------------

// Idle ticks (pulse periods) preceding each burst's run window: as large as
// the BMC's 16-bit period register allows (BMPER = idle + ncyc - 1), capped
// at BURST_IDLE_MAX so slow-period configurations don't inflate the burst
// frame (8192 periods is already ~4.3 s at the 524 us maximum period). For
// ncyc <= BURST_NCYC_MAX the result is at least BURST_IDLE_MIN.
static uint32_t burst_idle_ticks(uint32_t ncyc) {
  uint32_t idle = 65536U - ncyc;
  if (idle > BURST_IDLE_MAX)
    idle = BURST_IDLE_MAX;
  return idle;
}

// Arm one burst, then reschedule for the next repetition interval. At most
// one instance of this task is ever in the scheduler queue (there is no
// cancel, so a task per reconfiguration would leak queue slots until each
// stale one fired); apply_all() clears g_burst_outputs to make it die, and
// after a reconfiguration the surviving task arms the new configuration at
// its next firing.
static void burst_rep_task(void* data) {
  if (g_burst_outputs == 0) {
    g_burst_task_live = false;
    return;
  }
  schedule_us(g_burst_rep_us, burst_rep_task, NULL);

  // Glitch-free arm: with the outputs disabled, start the controller --
  // the burst begins in its idle phase -- then enable the outputs while
  // the controller holds them idle. The run window opens on the next
  // period roll-over after the idle window elapses.
  LL_HRTIM_DisableOutput(HRTIM1, g_burst_outputs);
  LL_HRTIM_ClearFlag_BMPER(HRTIM1);
  LL_HRTIM_BM_Enable(HRTIM1);
  LL_HRTIM_BM_Start(HRTIM1);
  LL_HRTIM_EnableOutput(HRTIM1, g_burst_outputs);
}

// BMPER fires at the end of the run window: the burst has emitted exactly
// its ncyc pulses and the controller has re-entered the idle phase (it runs
// in continuous mode and would otherwise start another frame). Park the
// bursting outputs and stop the controller; burst_rep_task re-arms at the
// next repetition. The deadline is the entire idle window -- at least
// BURST_IDLE_MIN periods (~49 us), typically milliseconds -- so this shares
// the master vector at a relaxed priority. BMPER, like the other HRTIM
// common-status interrupts, is routed to the master vector (RM0440
// "hrtim_it1").
void HRTIM1_Master_IRQHandler(void) {
  if (LL_HRTIM_IsActiveFlag_BMPER(HRTIM1)) {
    LL_HRTIM_DisableOutput(HRTIM1, g_burst_outputs);
    LL_HRTIM_BM_Disable(HRTIM1);
    LL_HRTIM_ClearFlag_BMPER(HRTIM1);
  }
}

// Program the burst mode controller for the bursting domain represented by
// ch and start the repetition schedule. The BMC is clocked by the domain's
// own period roll-over (the master's in sync mode), so its idle and run
// windows are exact integer counts of pulse periods aligned to period
// boundaries: each run window emits exactly ncyc whole pulses. MTBM/TxBM
// stay 0 (counters keep running during idle) -- stopping the counter would
// stop the BMC's own clock.
static void config_burst(int ch, uint32_t outputs) {
  const uint32_t ncyc = chan_burst_ncyc[ch];
  const uint32_t idle = burst_idle_ticks(ncyc);

  uint32_t clk_src;
  if (g_mode == MODE_SYNC) {
    clk_src = LL_HRTIM_BM_CLKSRC_MASTER;
  } else {
    clk_src = (channel_hw[ch].hrtim_timer == LL_HRTIM_TIMER_F) ? LL_HRTIM_BM_CLKSRC_TIMER_F
                                                               : LL_HRTIM_BM_CLKSRC_TIMER_E;
  }

  LL_HRTIM_BM_SetMode(HRTIM1, LL_HRTIM_BM_MODE_CONTINOUS);
  LL_HRTIM_BM_SetClockSrc(HRTIM1, clk_src);
  LL_HRTIM_BM_SetPrescaler(HRTIM1, LL_HRTIM_BM_PRESCALER_DIV1);
  LL_HRTIM_BM_DisablePreload(HRTIM1);
  // Idle for BMCMP+1 ticks, then run for the rest of the BMPER+1-tick
  // frame: run = BMPER - BMCMP = ncyc periods.
  LL_HRTIM_BM_SetCompare(HRTIM1, idle - 1);
  LL_HRTIM_BM_SetPeriod(HRTIM1, idle + ncyc - 1);

  g_burst_outputs = outputs;
  g_burst_rep_us = (Time)(chan_burst_rep_ps[ch] / 1000000);
  LL_HRTIM_EnableIT_BMPER(HRTIM1);
  if (!g_burst_task_live) {
    g_burst_task_live = true;
    schedule_us(1, burst_rep_task, NULL);
  }
}

// Validate one channel's parameters against the current mode. Returns 0 if OK
// (or if the channel is off -- an off channel imposes no constraint), else a
// negative code mapped to a SCPI error string by err_for_rc().
static int validate(int ch) {
  if (!chan_on[ch])
    return 0;
  const uint64_t T = chan_period_ps[ch];
  const uint64_t W = chan_width_ps[ch];
  const uint64_t D = chan_delay_ps[ch];
  if (!hrtim_dll_ready)
    return -3;
  if (T == 0 || W == 0)
    return -1;
  if (W >= T)
    return -2;
  if (D + W > T)
    return -5;
  uint8_t ck;
  uint32_t per;
  if (!select_ckpsc(T, &ck, &per))
    return -4;

  if (chan_burst_on[ch]) {
    const uint32_t n = chan_burst_ncyc[ch];
    if (n == 0 || n > BURST_NCYC_MAX)
      return -6;
    if (g_mode == MODE_ASYNC) {
      // A single burst mode controller serves the whole HRTIM, so only one
      // async group may burst. (Sync mode bursts all four channels
      // coherently off the master, so there is no conflict.)
      for (int i = 0; i < NUM_CHANNELS; i++) {
        if (!same_domain(ch, i) && chan_on[i] && chan_burst_on[i])
          return -7;
      }
    }
    if (chan_burst_rep_ps[ch] > BURST_REP_MAX_PS)
      return -9;
    // The frame (idle + run windows) is measured in quantized periods.
    const uint64_t frame_ps =
        (uint64_t)(burst_idle_ticks(n) + n) * per * (HRTIM_FINEST_TICK_PS << ck);
    if (chan_burst_rep_ps[ch] <= frame_ps + BURST_REP_MARGIN_PS)
      return -8;
  }
  return 0;
}

// Configure group g's timer and the outputs of its valid on-channels; park
// the rest low. Bursting channels are accumulated into *burst_outputs /
// *burst_ch instead of being enabled (the repetition task arms them per
// burst). Returns true iff anything on this group's timer runs.
static bool config_group(int g, uint8_t ck, uint32_t per, bool sync, uint32_t* burst_outputs,
                         int* burst_ch) {
  config_group_timer(g, ck, per, sync);
  bool group_runs = false;
  for (int s = 0; s < 2; s++) {
    const int ch = g * 2 + s;
    if (chan_on[ch] && validate(ch) == 0) {
      const bool burst = chan_burst_on[ch];
      config_channel_output(ch, ck, per, burst);
      if (burst) {
        *burst_outputs |= channel_hw[ch].hrtim_output;
        if (*burst_ch < 0)
          *burst_ch = ch;
      }
      group_runs = true;
    } else {
      drive_pin_low(ch);
    }
  }
  return group_runs;
}

// Tear down and rebuild the entire HRTIM configuration from the current state.
// Called after any parameter change. A channel that is off, or on but invalid,
// is parked low; valid on-channels drive their outputs.
static void apply_all(void) {
  // Quiesce the burst controller before touching any timer or output state.
  // Clearing g_burst_outputs makes a parked burst_rep_task die at its next
  // firing unless config_burst() re-arms below.
  g_burst_outputs = 0;
  LL_HRTIM_DisableIT_BMPER(HRTIM1);
  LL_HRTIM_BM_Disable(HRTIM1);
  LL_HRTIM_ClearFlag_BMPER(HRTIM1);

  // Stop master + both slave timers, disable all outputs.
  LL_HRTIM_TIM_CounterDisable(HRTIM1, LL_HRTIM_TIMER_MASTER | LL_HRTIM_TIMER_E | LL_HRTIM_TIMER_F);
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    LL_HRTIM_DisableOutput(HRTIM1, channel_hw[ch].hrtim_output);
  }

  if (!hrtim_dll_ready) {
    for (int ch = 0; ch < NUM_CHANNELS; ch++) drive_pin_low(ch);
    return;
  }

  uint32_t run_mask = 0;
  uint32_t burst_outputs = 0;
  int burst_ch = -1;

  if (g_mode == MODE_SYNC) {
    // One global period (all chan_period_ps are kept equal in sync mode).
    const bool any_on = chan_on[0] || chan_on[1] || chan_on[2] || chan_on[3];
    uint8_t ck;
    uint32_t per;
    if (any_on && select_ckpsc(chan_period_ps[0], &ck, &per)) {
      // Master timer defines the period and resets both slaves each cycle.
      // It must run continuously: the default single-shot mode would count
      // one period and stop, so its period event (the slaves' reset source
      // and the burst controller's clock) would fire only once.
      LL_HRTIM_TIM_SetPrescaler(HRTIM1, LL_HRTIM_TIMER_MASTER, hrtim_ckpsc_to_ll[ck]);
      LL_HRTIM_TIM_SetCounterMode(HRTIM1, LL_HRTIM_TIMER_MASTER,
                                  LL_HRTIM_MODE_CONTINUOUS);
      LL_HRTIM_TIM_SetPeriod(HRTIM1, LL_HRTIM_TIMER_MASTER, per);
      LL_HRTIM_TIM_SetRepetition(HRTIM1, LL_HRTIM_TIMER_MASTER, 0);
      run_mask = LL_HRTIM_TIMER_MASTER;

      for (int g = 0; g < 2; g++) {
        if (config_group(g, ck, per, /*sync=*/true, &burst_outputs, &burst_ch)) {
          run_mask |= group_timer[g];
        }
      }
    } else {
      for (int ch = 0; ch < NUM_CHANNELS; ch++) drive_pin_low(ch);
    }
  } else {
    // Async: each group is an independent free-running timer.
    for (int g = 0; g < 2; g++) {
      int base = g * 2;
      bool group_on = chan_on[base] || chan_on[base + 1];
      uint8_t ck;
      uint32_t per;
      if (!group_on || !select_ckpsc(chan_period_ps[base], &ck, &per)) {
        drive_pin_low(base);
        drive_pin_low(base + 1);
        continue;
      }
      if (config_group(g, ck, per, /*sync=*/false, &burst_outputs, &burst_ch)) {
        run_mask |= group_timer[g];
      }
    }
  }

  if (run_mask) {
    // Zero every counter before enabling: LL_HRTIM_TIM_CounterDisable above
    // stops the counters but leaves CNT at its last value, so without this a
    // re-enable would resume from a stale, per-timer-different count. Reset
    // to 0 so all enabled timers start the same cycle phase-aligned -- in
    // SYNC mode that is what makes Timer E coincide with Timer F (and the
    // master); they share one clock and period, so an aligned start stays
    // aligned.
    LL_HRTIM_CounterReset(HRTIM1, run_mask);
    LL_HRTIM_TIM_CounterEnable(HRTIM1, run_mask);
  }
  // The bursting domain's counter must already be running: it clocks the
  // burst mode controller.
  if (burst_ch >= 0)
    config_burst(burst_ch, burst_outputs);
}

// ---- SCPI hooks (called from scpi.c) ---------------------------------------

// Translate a validate() code into a SCPI error string, or NULL on success.
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
      return "-200,\"Period out of range (24 ns .. 524 us)\"";
    case -5:
      return "-200,\"Delay + width must be <= period\"";
    case -6:
      return "-200,\"Burst count out of range (1 .. 63488)\"";
    case -7:
      return "-200,\"Only one group can burst in ASYNC mode\"";
    case -8:
      return "-200,\"Burst interval too short (needs burst length"
             " + 50 ms)\"";
    case -9:
      return "-200,\"Burst interval out of range (max 60 s)\"";
    default:
      return "-200,\"Apply failed\"";
  }
}

// Channels sharing a domain must share a period: the group in async, all four
// in sync. Propagate so each channel's stored period is always its effective
// period.
static void set_domain_period(int ch, uint64_t ps) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (same_domain(ch, i))
      chan_period_ps[i] = ps;
  }
}

const char* pulsegen_set_state(int ch, bool on) {
  chan_on[ch] = on;
  apply_all();
  return err_for_rc(validate(ch));
}

const char* pulsegen_set_period_ps(int ch, uint64_t ps) {
  set_domain_period(ch, ps);
  apply_all();
  return err_for_rc(validate(ch));
}

const char* pulsegen_set_width_ps(int ch, uint64_t ps) {
  chan_width_ps[ch] = ps;
  apply_all();
  return err_for_rc(validate(ch));
}

const char* pulsegen_set_delay_ps(int ch, uint64_t ps) {
  chan_delay_ps[ch] = ps;
  apply_all();
  return err_for_rc(validate(ch));
}

const char* pulsegen_set_burst_state(int ch, bool on) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (same_domain(ch, i))
      chan_burst_on[i] = on;
  }
  apply_all();
  return err_for_rc(validate(ch));
}

const char* pulsegen_set_burst_ncyc(int ch, uint32_t ncyc) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (same_domain(ch, i))
      chan_burst_ncyc[i] = ncyc;
  }
  apply_all();
  return err_for_rc(validate(ch));
}

const char* pulsegen_set_burst_period_ps(int ch, uint64_t ps) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (same_domain(ch, i))
      chan_burst_rep_ps[i] = ps;
  }
  apply_all();
  return err_for_rc(validate(ch));
}

bool pulsegen_get_burst_state(int ch) {
  return chan_burst_on[ch];
}

uint32_t pulsegen_get_burst_ncyc(int ch) {
  return chan_burst_ncyc[ch];
}

uint64_t pulsegen_get_burst_period_ps(int ch) {
  return chan_burst_rep_ps[ch];
}

const char* pulsegen_set_mode(bool sync) {
  g_mode = sync ? MODE_SYNC : MODE_ASYNC;
  // Sync collapses the device into a single domain; adopt channel 0's
  // domain settings for all.
  if (sync) {
    for (int i = 1; i < NUM_CHANNELS; i++) {
      chan_period_ps[i] = chan_period_ps[0];
      chan_burst_on[i] = chan_burst_on[0];
      chan_burst_ncyc[i] = chan_burst_ncyc[0];
      chan_burst_rep_ps[i] = chan_burst_rep_ps[0];
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
    chan_period_ps[i] = 0;
    chan_width_ps[i] = 0;
    chan_delay_ps[i] = 0;
    chan_on[i] = false;
    chan_burst_on[i] = false;
    chan_burst_ncyc[i] = 1;
    chan_burst_rep_ps[i] = BURST_REP_DEFAULT_PS;
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

  // Channel LED blinks at the heartbeat rate while the channel is actively
  // outputting. Pulses go out at MHz rates -- far too fast to blink per pulse
  // -- so this is the activity analog of the timestamper's per-pulse blink.
  for (int i = 0; i < NUM_CHANNELS; i++) {
    gpio_set_or_clr(channel_hw[i].led, chan_on[i] && on_phase);
  }

  bool usb_blink_off = !on_phase && scpi_consume_recent_activity();
  gpio_set_or_clr(LED_USB, scpi_usb_ready() && !usb_blink_off);
}

// ---- Periodic task ---------------------------------------------------------

static void periodic_task(void* data) {
  schedule_us(100000, periodic_task, NULL);

  if (g_rulos_hse_failed) {
    // Disable outputs and flash the channel LEDs as a visual indicator.
    for (int i = 0; i < NUM_CHANNELS; i++) {
      chan_on[i] = false;
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

  // Continuous DLL recalibration: refreshes against process/temp drift.
  LL_HRTIM_ConfigDLLCalibration(HRTIM1, LL_HRTIM_DLLCALIBRATION_MODE_CONTINUOUS,
                                LL_HRTIM_DLLCALIBRATION_RATE_3);
  // Wait for the first calibration to complete (~6 us per RM0440); poll
  // the DLLRDY flag rather than blindly delaying.
  for (uint32_t spin = 0; spin < 100000; spin++) {
    if (LL_HRTIM_IsActiveFlag_DLLRDY(HRTIM1)) {
      hrtim_dll_ready = true;
      break;
    }
  }

  // Burst-end (BMPER) interrupt. Its deadline is a whole burst idle window
  // (>= ~49 us), so it runs at a relaxed priority.
  HAL_NVIC_SetPriority(HRTIM1_Master_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(HRTIM1_Master_IRQn);
}

static void init_gpio(void) {
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);

  // Channel pins start parked low.
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

  // USART1 on PA9/PA10 (rulos uart_id=0).
  uart_init(&uart, /*uart_id=*/0, 1000000);
  log_bind_uart(&uart);

  init_gpio();

  pulsegen_scpi_init();

  init_hrtim();

  // Establish power-on defaults (notably the burst parameters, whose
  // defaults are nonzero) -- the same state *RST restores.
  pulsegen_reset_all();

  schedule_us(1, periodic_task, NULL);
  scheduler_run();
}
