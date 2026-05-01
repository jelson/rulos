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
 * pulsegen: a 2-channel pulse generator built on the same hardware family as
 * timestamper but using the STM32G474, which has the High-Resolution Timer
 * (HRTIM) peripheral. Configuration is delivered over USB CDC using a small
 * subset of SCPI; a UART debug port stays available for log output.
 *
 * Clock plan (set in SConstruct via PLLM/PLLN):
 *   HSE 10 MHz / PLLM=1 * PLLN=25 / PLLR=2 = 125 MHz SYSCLK
 *   fHRTIM = 125 MHz; with the HRTIM DLL at 32x oversampling, fHRCK = 4 GHz
 *   and the finest tick is 250 ps.
 *
 * Output pins -- chosen because they are the *only* pins on the G474 that map
 * to both an HRTIM output and a 32-bit timer (TIM2) channel, allowing the
 * same physical pin to carry either a fine HRTIM pulse train or a long-period
 * TIM2 pulse train depending on the requested rate:
 *
 *   Channel 1 -> PA9   (HRTIM1_CHA2 = AF13, or TIM2_CH3 = AF10)
 *   Channel 2 -> PA10  (HRTIM1_CHB1 = AF13, or TIM2_CH4 = AF10)
 *
 * Because PA9/PA10 are also USART1, the debug UART moves to USART2 (PA2 TX /
 * PA3 RX, RULOS uart_id=1).
 *
 * Timebase ladder. For a requested period T, we pick the highest-resolution
 * clock source that can still represent T in the available counter width:
 *
 *   HRTIM CKPSC=0 (mul32):   250 ps tick  --> max ~16.4 us  (16-bit ARR)
 *   HRTIM CKPSC=1 (mul16):   500 ps tick  --> max ~32.8 us
 *   HRTIM CKPSC=2 (mul8):    1   ns tick  --> max ~65.5 us
 *   HRTIM CKPSC=3 (mul4):    2   ns tick  --> max ~131  us
 *   HRTIM CKPSC=4 (mul2):    4   ns tick  --> max ~262  us
 *   HRTIM CKPSC=5 (div1):    8   ns tick  --> max ~524  us
 *   TIM2 (32-bit @ sysclk):  8   ns tick  --> max ~34.4 s
 *
 * Caveat: TIM2_CH3 and TIM2_CH4 share the TIM2 ARR. If both channels need
 * TIM2 mode simultaneously, the second configuration overrides the period of
 * the first. (Width is per-channel via CCR3/CCR4 and is not affected.) This
 * only matters above 524 us, where the system is already constrained.
 *
 * SCPI command surface (subset):
 *   *IDN?
 *   *RST
 *   OUTPut[1|2]:STATe ON|OFF
 *   SOURce[1|2]:PULSe:PERiod  <seconds>
 *   SOURce[1|2]:PULSe:WIDTh   <seconds>
 *   SOURce[1|2]:PULSe:COUNt   <n>           (0 = continuous)
 *   SOURce[1|2]:BURSt:PERiod  <seconds>     (0 = single shot when COUN > 0)
 *   SYSTem:ERRor?
 *
 * Burst implementation. Continuous mode (COUN=0) just runs the timer. For
 * finite COUN the soft scheduler arms the output every BURSt:PERiod and
 * disables it after COUN*PERiod seconds. This is accurate to scheduler jitter
 * (~100 us). HRTIM single-shot/repetition-counter accuracy is a future
 * improvement.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/scpi/scpi.h"
#include "periph/uart/uart.h"
#include "pulsegen.h"
#include "scpi.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_hrtim.h"
#include "stm32g4xx_ll_tim.h"

#ifndef RULOS_USE_HSE
#error "pulsegen requires RULOS_USE_HSE"
#endif

// NUM_CHANNELS comes from pulsegen.h.
#define SYSCLK_HZ     125000000UL
#define HRTIM_CLK_HZ  125000000UL  // fHRTIM == sysclk on G4
// fHRCK at CKPSC=0 = 32 * 125 MHz = 4 GHz -> 250 ps/tick
#define HRTIM_FINEST_TICK_PS 250ULL
// TIM2 runs at sysclk -> 1/125 MHz = 8000 ps/tick
#define TIM2_TICK_PS         8000ULL

// HRTIM 16-bit period max in high-res mode (RM0440 27.3.4): the period must
// be at least 3 (or higher in mul* modes) and at most 0xFFDF.
#define HRTIM_PER_MAX 0xFFDFU

// LEDs. PF1 is normally OSC_OUT; HSE bypass mode frees it for GPIO.
#define LED_CHAN0 GPIO_A4   // channel 1 activity (output transitioning)
#define LED_CHAN1 GPIO_A5   // channel 2 activity
#define LED_CLOCK GPIO_F1   // 10 MHz HSE health
#define LED_USB   GPIO_A8   // USB activity

// ---- Channel hardware mapping ----------------------------------------------

typedef struct {
  // GPIO pin (always GPIOA bit n on this board)
  uint32_t ll_pin;            // LL_GPIO_PIN_n
  uint32_t hrtim_timer;       // LL_HRTIM_TIMER_A / B
  uint32_t hrtim_output;      // LL_HRTIM_OUTPUT_TA2 / TB1
  uint32_t tim2_ll_channel;   // LL_TIM_CHANNEL_CH3 / CH4
  volatile uint32_t *tim2_ccr;
  gpio_pin_t led;
} channel_hw_t;

static const channel_hw_t channel_hw[NUM_CHANNELS] = {
    [0] = {
        .ll_pin           = LL_GPIO_PIN_9,
        .hrtim_timer      = LL_HRTIM_TIMER_A,
        .hrtim_output     = LL_HRTIM_OUTPUT_TA2,
        .tim2_ll_channel  = LL_TIM_CHANNEL_CH3,
        .tim2_ccr         = &TIM2->CCR3,
        .led              = LED_CHAN0,
    },
    [1] = {
        .ll_pin           = LL_GPIO_PIN_10,
        .hrtim_timer      = LL_HRTIM_TIMER_B,
        .hrtim_output     = LL_HRTIM_OUTPUT_TB1,
        .tim2_ll_channel  = LL_TIM_CHANNEL_CH4,
        .tim2_ccr         = &TIM2->CCR4,
        .led              = LED_CHAN1,
    },
};

// ---- Channel runtime state -------------------------------------------------

typedef enum {
  CHAN_OFF = 0,
  CHAN_HRTIM,
  CHAN_TIM2,
} chan_mode_t;

typedef struct {
  // User-configured parameters. Periods/widths held in picoseconds (fits in
  // uint64_t with plenty of headroom; 1 second = 1e12 ps).
  uint64_t period_ps;
  uint64_t width_ps;
  uint32_t count;             // 0 = continuous
  uint64_t burst_period_ps;   // 0 with count>0 = single shot
  bool requested_on;

  // Resolved timebase
  chan_mode_t mode;

  // For burst scheduling: true between burst start and burst end.
  bool bursting;
} channel_state_t;

static channel_state_t chan_state[NUM_CHANNELS];

// ---- Globals ---------------------------------------------------------------

static UartState_t uart;
static volatile bool chan_active_pulse[NUM_CHANNELS];
static bool welcome_printed = false;
static bool hrtim_dll_ready = false;

// Last assigned TIM2 ARR; used to detect inter-channel conflict.
static uint32_t tim2_active_arr = 0;
static uint8_t tim2_active_mask = 0;  // bit n set if channel n is in TIM2 mode

// ---- HRTIM / TIM2 helpers --------------------------------------------------

static const uint32_t hrtim_ckpsc_to_ll[6] = {
    LL_HRTIM_PRESCALERRATIO_MUL32,
    LL_HRTIM_PRESCALERRATIO_MUL16,
    LL_HRTIM_PRESCALERRATIO_MUL8,
    LL_HRTIM_PRESCALERRATIO_MUL4,
    LL_HRTIM_PRESCALERRATIO_MUL2,
    LL_HRTIM_PRESCALERRATIO_DIV1,
};

// Per-CKPSC minimum PER value (RM0440 Table 218, "Minimum PER value").
// Numbers come from the high-resolution-mode minimums; div1 (CKPSC=5) has a
// looser min of 3.
static const uint16_t hrtim_min_per[6] = {
    0x0060, 0x0030, 0x0018, 0x000C, 0x0006, 0x0003,
};

// Fill ckpsc/arr/ccr for a request that fits in HRTIM. Returns false if no
// CKPSC works (period either too short or too long for HRTIM).
static bool select_hrtim(uint64_t period_ps, uint64_t width_ps,
                         uint8_t *out_ckpsc, uint32_t *out_arr,
                         uint32_t *out_ccr) {
  for (uint8_t k = 0; k <= 5; k++) {
    uint64_t tick_ps = HRTIM_FINEST_TICK_PS << k;
    uint64_t arr = period_ps / tick_ps;
    if (arr > HRTIM_PER_MAX) continue;
    if (arr < hrtim_min_per[k]) continue;
    uint64_t ccr = width_ps / tick_ps;
    // Compare value must be within (min..arr-1) for a clean PWM edge.
    if (ccr < 3) ccr = 3;
    if (ccr >= arr) ccr = arr - 1;
    *out_ckpsc = k;
    *out_arr = (uint32_t)arr;
    *out_ccr = (uint32_t)ccr;
    return true;
  }
  return false;
}

// TIM2 fallback: 32-bit counter at sysclk = 8000 ps/tick.
static bool select_tim2(uint64_t period_ps, uint64_t width_ps,
                        uint32_t *out_arr, uint32_t *out_ccr) {
  uint64_t arr = period_ps / TIM2_TICK_PS;
  if (arr < 2 || arr > 0xFFFFFFFFULL) return false;
  uint64_t ccr = width_ps / TIM2_TICK_PS;
  if (ccr < 1) ccr = 1;
  if (ccr >= arr) ccr = arr - 1;
  *out_arr = (uint32_t)arr;
  *out_ccr = (uint32_t)ccr;
  return true;
}

static void set_pin_af(int ch, uint8_t af) {
  // PA9/PA10 are both in the high half (pins 8..15)
  LL_GPIO_SetAFPin_8_15(GPIOA, channel_hw[ch].ll_pin, af);
  LL_GPIO_SetPinMode(GPIOA, channel_hw[ch].ll_pin, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetPinSpeed(GPIOA, channel_hw[ch].ll_pin,
                      LL_GPIO_SPEED_FREQ_VERY_HIGH);
  LL_GPIO_SetPinOutputType(GPIOA, channel_hw[ch].ll_pin,
                           LL_GPIO_OUTPUT_PUSHPULL);
  LL_GPIO_SetPinPull(GPIOA, channel_hw[ch].ll_pin, LL_GPIO_PULL_NO);
}

static void drive_pin_low(int ch) {
  LL_GPIO_ResetOutputPin(GPIOA, channel_hw[ch].ll_pin);
  LL_GPIO_SetPinMode(GPIOA, channel_hw[ch].ll_pin, LL_GPIO_MODE_OUTPUT);
  LL_GPIO_SetPinOutputType(GPIOA, channel_hw[ch].ll_pin,
                           LL_GPIO_OUTPUT_PUSHPULL);
}

// Stop whatever is currently driving the pin and park it low.
static void stop_channel_output(int ch) {
  const channel_hw_t *hw = &channel_hw[ch];
  channel_state_t *st = &chan_state[ch];

  switch (st->mode) {
    case CHAN_HRTIM:
      LL_HRTIM_DisableOutput(HRTIM1, hw->hrtim_output);
      LL_HRTIM_TIM_CounterDisable(HRTIM1, hw->hrtim_timer);
      break;
    case CHAN_TIM2:
      LL_TIM_CC_DisableChannel(TIM2, hw->tim2_ll_channel);
      tim2_active_mask &= ~(1U << ch);
      if (tim2_active_mask == 0) {
        LL_TIM_DisableCounter(TIM2);
        tim2_active_arr = 0;
      }
      break;
    case CHAN_OFF:
    default:
      break;
  }
  st->mode = CHAN_OFF;
  drive_pin_low(ch);
}

static void start_hrtim(int ch, uint8_t ckpsc, uint32_t arr, uint32_t ccr) {
  const channel_hw_t *hw = &channel_hw[ch];

  LL_HRTIM_TIM_CounterDisable(HRTIM1, hw->hrtim_timer);
  LL_HRTIM_DisableOutput(HRTIM1, hw->hrtim_output);

  LL_HRTIM_TIM_SetPrescaler(HRTIM1, hw->hrtim_timer, hrtim_ckpsc_to_ll[ckpsc]);
  LL_HRTIM_TIM_SetCounterMode(HRTIM1, hw->hrtim_timer, LL_HRTIM_MODE_CONTINUOUS);
  LL_HRTIM_TIM_SetPeriod(HRTIM1, hw->hrtim_timer, arr);
  LL_HRTIM_TIM_SetRepetition(HRTIM1, hw->hrtim_timer, 0);
  LL_HRTIM_TIM_SetCompare1(HRTIM1, hw->hrtim_timer, ccr);
  LL_HRTIM_TIM_SetUpdateGating(HRTIM1, hw->hrtim_timer,
                               LL_HRTIM_UPDATEGATING_INDEPENDENT);
  LL_HRTIM_TIM_SetCountingMode(HRTIM1, hw->hrtim_timer,
                               LL_HRTIM_COUNTING_MODE_UP);
  LL_HRTIM_TIM_DisablePreload(HRTIM1, hw->hrtim_timer);

  // High at start of period, low at compare-1: standard PWM.
  LL_HRTIM_OUT_SetPolarity(HRTIM1, hw->hrtim_output,
                           LL_HRTIM_OUT_POSITIVE_POLARITY);
  LL_HRTIM_OUT_SetOutputSetSrc(HRTIM1, hw->hrtim_output,
                               LL_HRTIM_OUTPUTSET_TIMPER);
  LL_HRTIM_OUT_SetOutputResetSrc(HRTIM1, hw->hrtim_output,
                                 LL_HRTIM_OUTPUTRESET_TIMCMP1);
  LL_HRTIM_OUT_SetIdleMode(HRTIM1, hw->hrtim_output, LL_HRTIM_OUT_NO_IDLE);
  LL_HRTIM_OUT_SetIdleLevel(HRTIM1, hw->hrtim_output,
                            LL_HRTIM_OUT_IDLELEVEL_INACTIVE);
  LL_HRTIM_OUT_SetFaultState(HRTIM1, hw->hrtim_output,
                             LL_HRTIM_OUT_FAULTSTATE_NO_ACTION);
  LL_HRTIM_OUT_SetChopperMode(HRTIM1, hw->hrtim_output,
                              LL_HRTIM_OUT_CHOPPERMODE_DISABLED);

  set_pin_af(ch, 13);  // AF13 = HRTIM
  LL_HRTIM_EnableOutput(HRTIM1, hw->hrtim_output);
  LL_HRTIM_TIM_CounterEnable(HRTIM1, hw->hrtim_timer);
}

static void start_tim2(int ch, uint32_t arr, uint32_t ccr) {
  const channel_hw_t *hw = &channel_hw[ch];

  // Enforce shared-ARR semantics. If another channel is already running TIM2
  // with a different ARR, the latest configuration wins (caller already
  // warned on the SCPI side).
  LL_TIM_CC_DisableChannel(TIM2, hw->tim2_ll_channel);

  LL_TIM_SetAutoReload(TIM2, arr - 1);  // ARR is "period - 1"
  *hw->tim2_ccr = ccr;

  LL_TIM_OC_SetMode(TIM2, hw->tim2_ll_channel, LL_TIM_OCMODE_PWM1);
  LL_TIM_OC_SetPolarity(TIM2, hw->tim2_ll_channel, LL_TIM_OCPOLARITY_HIGH);
  LL_TIM_OC_EnablePreload(TIM2, hw->tim2_ll_channel);
  LL_TIM_CC_EnableChannel(TIM2, hw->tim2_ll_channel);

  set_pin_af(ch, 10);  // AF10 = TIM2

  if (!LL_TIM_IsEnabledCounter(TIM2)) {
    LL_TIM_GenerateEvent_UPDATE(TIM2);  // load preload
    LL_TIM_EnableCounter(TIM2);
  }
  tim2_active_arr = arr;
  tim2_active_mask |= (1U << ch);
}

// ---- Channel apply ---------------------------------------------------------

// Called when the user toggles a channel state, or when burst arming wants
// the output running. Picks HRTIM vs TIM2 and configures hardware. Returns
// 0 on success, negative on error (caller logs SCPI error).
static int apply_channel_active(int ch) {
  channel_state_t *st = &chan_state[ch];
  const uint64_t T = st->period_ps;
  const uint64_t W = st->width_ps;

  if (T == 0 || W == 0) return -1;
  if (W >= T) return -2;

  uint8_t ckpsc;
  uint32_t arr, ccr;
  if (select_hrtim(T, W, &ckpsc, &arr, &ccr)) {
    stop_channel_output(ch);
    if (!hrtim_dll_ready) return -3;
    start_hrtim(ch, ckpsc, arr, ccr);
    st->mode = CHAN_HRTIM;
    chan_active_pulse[ch] = true;
    return 0;
  }
  if (select_tim2(T, W, &arr, &ccr)) {
    stop_channel_output(ch);
    start_tim2(ch, arr, ccr);
    st->mode = CHAN_TIM2;
    chan_active_pulse[ch] = true;
    return 0;
  }
  return -4;  // can't represent this period at all
}

// Burst-mode soft scheduler. For COUN > 0, output is gated: armed for
// COUN*PERiod seconds at every BURSt:PERiod boundary.

static void burst_off_callback(void *data);
static void burst_on_callback(void *data);

static void schedule_burst_off(int ch) {
  channel_state_t *st = &chan_state[ch];
  // (count * period) in microseconds. period_ps/1000000 = us.
  uint64_t burst_us =
      (st->period_ps / 1000000ULL) * (uint64_t)st->count;
  if (burst_us == 0) burst_us = 1;
  schedule_us((Time)burst_us, burst_off_callback,
              (void *)(uintptr_t)ch);
}

static void schedule_burst_on(int ch) {
  channel_state_t *st = &chan_state[ch];
  if (st->burst_period_ps == 0) return;  // single shot
  uint64_t period_us = st->burst_period_ps / 1000000ULL;
  if (period_us == 0) period_us = 1;
  schedule_us((Time)period_us, burst_on_callback,
              (void *)(uintptr_t)ch);
}

static void burst_off_callback(void *data) {
  int ch = (int)(uintptr_t)data;
  channel_state_t *st = &chan_state[ch];
  if (!st->bursting) return;
  stop_channel_output(ch);
  st->bursting = false;
  if (st->requested_on) {
    schedule_burst_on(ch);
  }
}

static void burst_on_callback(void *data) {
  int ch = (int)(uintptr_t)data;
  channel_state_t *st = &chan_state[ch];
  if (!st->requested_on) return;
  if (apply_channel_active(ch) != 0) return;
  st->bursting = true;
  schedule_burst_off(ch);
}

static int apply_channel(int ch) {
  channel_state_t *st = &chan_state[ch];
  st->bursting = false;

  if (!st->requested_on) {
    stop_channel_output(ch);
    return 0;
  }

  if (st->count == 0) {
    return apply_channel_active(ch);
  }

  // Burst mode: arm now, then schedule off + (optional) re-arm.
  int rc = apply_channel_active(ch);
  if (rc != 0) return rc;
  st->bursting = true;
  schedule_burst_off(ch);
  return 0;
}

// ---- SCPI hooks (called from scpi.c) ---------------------------------------

const char *const pulsegen_idn = "RULOS,Pulsegen,0," STRINGIFY(GIT_COMMIT);

// Translate apply_channel's status code into a SCPI error string, or NULL on
// success. Returned strings have static storage.
static const char *err_for_apply_rc(int rc) {
  switch (rc) {
    case 0:  return NULL;
    case -1: return "-200,\"Period and width must be > 0\"";
    case -2: return "-200,\"Width must be < period\"";
    case -3: return "-200,\"HRTIM DLL not ready\"";
    case -4: return "-200,\"Period out of range\"";
    default: return "-200,\"Apply failed\"";
  }
}

// If the channel is currently armed, push the new config to hardware. Used
// by every parameter setter so changes take effect immediately while
// running. While off, parameter changes are stored and applied at the next
// OUTP:STAT ON.
static const char *reapply_if_on(int ch) {
  if (!chan_state[ch].requested_on) return NULL;
  return err_for_apply_rc(apply_channel(ch));
}

const char *pulsegen_set_state(int ch, bool on) {
  chan_state[ch].requested_on = on;
  return err_for_apply_rc(apply_channel(ch));
}

const char *pulsegen_set_period_ps(int ch, uint64_t ps) {
  chan_state[ch].period_ps = ps;
  return reapply_if_on(ch);
}

const char *pulsegen_set_width_ps(int ch, uint64_t ps) {
  chan_state[ch].width_ps = ps;
  return reapply_if_on(ch);
}

const char *pulsegen_set_count(int ch, uint32_t n) {
  chan_state[ch].count = n;
  return reapply_if_on(ch);
}

const char *pulsegen_set_burst_period_ps(int ch, uint64_t ps) {
  chan_state[ch].burst_period_ps = ps;
  return NULL;  // takes effect at the next burst-on boundary
}

void pulsegen_reset_all(void) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    chan_state[i] = (channel_state_t){0};
    stop_channel_output(i);
  }
}

// ---- LEDs ------------------------------------------------------------------

static bool received_recent_chan_pulse(int ch) {
  if (chan_active_pulse[ch]) {
    chan_active_pulse[ch] = false;
    return true;
  }
  return false;
}

static void update_leds(void) {
  static bool on_phase = false;
  on_phase = !on_phase;

  if (g_rulos_hse_failed) {
    gpio_clr(LED_CLOCK);
  } else {
    gpio_set_or_clr(LED_CLOCK, on_phase);
  }

  for (int i = 0; i < NUM_CHANNELS; i++) {
    bool blink = received_recent_chan_pulse(i);
    // Channel LED is on whenever the channel is actively running, plus a
    // quick blink when state changes.
    bool active = (chan_state[i].mode != CHAN_OFF) ||
                  (chan_state[i].requested_on && chan_state[i].bursting);
    gpio_set_or_clr(channel_hw[i].led, active || (on_phase && blink));
  }

  bool usb_blink_off = !on_phase && scpi_consume_recent_activity();
  gpio_set_or_clr(LED_USB, scpi_usb_ready() && !usb_blink_off);
}

// ---- Periodic task ---------------------------------------------------------

static void periodic_task(void *data) {
  schedule_us(100000, periodic_task, NULL);

  if (g_rulos_hse_failed) {
    // Disable any output
    for (int i = 0; i < NUM_CHANNELS; i++) {
      stop_channel_output(i);
    }
    static bool on = false;
    on = !on;
    gpio_clr(LED_CLOCK);
    for (int i = 0; i < NUM_CHANNELS; i++) {
      gpio_set_or_clr(channel_hw[i].led, on);
    }
    static bool error_printed = false;
    if (!error_printed && scpi_usb_ready()) {
      error_printed = true;
      scpi_print(
          "# FATAL: External oscillator failure. Connect a 10MHz source"
          " and press reset.");
    }
    return;
  }

  if (!welcome_printed && scpi_usb_ready()) {
    scpi_print("# pulsegen ready, version " STRINGIFY(GIT_COMMIT));
    welcome_printed = true;
  }

  update_leds();
}

// ---- Init ------------------------------------------------------------------

static void init_hrtim(void) {
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_HRTIM1);

  // Continuous DLL recalibration: refreshes against process/temp drift.
  LL_HRTIM_ConfigDLLCalibration(HRTIM1,
                                LL_HRTIM_DLLCALIBRATION_MODE_CONTINUOUS,
                                LL_HRTIM_DLLCALIBRATION_RATE_3);
  // Wait for the first calibration to complete (~6 us per RM0440); poll
  // the DLLRDY flag rather than blindly delaying.
  for (uint32_t spin = 0; spin < 100000; spin++) {
    if (LL_HRTIM_IsActiveFlag_DLLRDY(HRTIM1)) {
      hrtim_dll_ready = true;
      break;
    }
  }
}

static void init_tim2(void) {
  __HAL_RCC_TIM2_CLK_ENABLE();
  LL_TIM_InitTypeDef tinit = {
      .Prescaler = 0,
      .CounterMode = LL_TIM_COUNTERMODE_UP,
      .Autoreload = 0xFFFFFFFFU,
      .ClockDivision = LL_TIM_CLOCKDIVISION_DIV1,
      .RepetitionCounter = 0,
  };
  LL_TIM_Init(TIM2, &tinit);
  LL_TIM_DisableARRPreload(TIM2);
  LL_TIM_EnableARRPreload(TIM2);
  // Counter starts disabled; start_tim2 enables on demand.
}

static void init_gpio(void) {
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOF);

  // Channel pins start parked low.
  for (int i = 0; i < NUM_CHANNELS; i++) {
    drive_pin_low(i);
  }

  gpio_make_output(LED_CHAN0);
  gpio_make_output(LED_CHAN1);
  gpio_make_output(LED_CLOCK);
  gpio_make_output(LED_USB);
}

int main(void) {
  rulos_hal_init();

  init_clock(10000, TIMER1);

  // Channel state defaults: outputs off, no period configured.
  memset(chan_state, 0, sizeof(chan_state));

  // USART2 on PA2/PA3 (rulos uart_id=1) -- USART1 (PA9/PA10) is reserved
  // for the pulse outputs.
  uart_init(&uart, /*uart_id=*/1, 1000000);
  log_bind_uart(&uart);

  init_gpio();

  pulsegen_scpi_init();

  init_hrtim();
  init_tim2();

  schedule_us(1, periodic_task, NULL);
  scheduler_run();
}
