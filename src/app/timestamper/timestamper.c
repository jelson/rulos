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
 * This program timestamps pulses that it sees on its input capture pin and
 * prints a list of those timestamps (relative to program start) on the serial
 * port. It has a resolution of 6 nanoseconds.
 *
 * It is based on the STM32G431x8, a wonderful little CPU that costs $5 but can
 * run at 170Mhz. The software assumes that a 10Mhz clock reference is provided
 * on OSC_IN pin (pin 2). In my testing, I used a surplus rubidium oscillator
 * with an unbiased [diode clamper]
 * (https://en.wikipedia.org/wiki/Clamper_(electronics)) used to shift the
 * zero-centered signal up so that it does not go much below ground. A Schottky
 * diode is crucial to keep the voltage from dipping too far below ground; the
 * size of the diode drop will be about how far below ground the shifted signal
 * goes.
 *
 * The input signal should be provided on PA0 (pin 5).
 *
 * Output is written to the UART at 1Mbps. Output is simply a list of
 * timestamps, one per line. Each is in decimal seconds, with 12 digits
 * following the decimal point representing picoseconds. Though the current
 * version only has 9ns resolution, future versions are expected to have 184ps
 * resolution, and emitting picoseconds avoids quantization effects. Timestamps
 * are relative to the time the program started.
 *
 * The implementation uses input-capture feature of TIM2, a 32-bit timer of the
 * STM32G431. Input capture waits for the rising edge of the input signal and
 * then latches the timer value at the moment of the signal's edge. The timer
 * runs at the system clock rate of 170MHz. The 170MHz clock is achieved by
 * using the STM32's PLL to multiply the 10MHz input reference frequency by
 * 17. The timer is configured to roll over once per second, i.e. every 170
 * million clock ticks. On each rollover interrupt, the program increments a
 * counter of the total number of seconds that have elapsed. On each input
 * capture interrupt, the latched timer value is read and converted from the
 * timer scale of one tick per 170-millionths of a second to picoseconds, by
 * multiplying it by 100,000 and then dividing it by 17. (64-bit math is used
 * for this to prevent overflow.)
 *
 * RACE CONDITION AVOIDANCE WITH seconds_A / seconds_B:
 *
 * A timestamp consists of two parts: the high-order "seconds" counter
 * (maintained in software) and the low-order timer value (latched in hardware
 * at the moment of input capture). There's a potential race condition: if the
 * timer rolls over between when an input capture occurs and when the ISR reads
 * the seconds counter, the wrong seconds value could be paired with the timer
 * value.
 *
 * To solve this, we maintain two copies of the seconds counter (seconds_A and
 * seconds_B) and use a separate timer (TIM15) that fires twice per TIM2 cycle:
 * at the 1/4 and 3/4 points of each second. The TIM15 ISR checks TIM2's current
 * value to determine which firing it is:
 *
 * - At the 3/4 point (TIM2 counter is in second half): increment seconds_A,
 *   preparing it for the upcoming rollover
 * - At the 1/4 point (TIM2 counter is in first half): copy seconds_A to
 *   seconds_B, syncing them for the new second
 *
 * In the input capture ISR, we check the captured timer value:
 * - If counter < half: use seconds_A (event was in the first half of the second)
 * - If counter >= half: use seconds_B (event was in the second half)
 *
 * This ensures correctness even if there's a delay between input capture and
 * ISR execution. For example, if an input capture occurs just before rollover
 * (counter near max) but the ISR runs just after rollover (when seconds_A has
 * already been incremented), the ISR sees counter >= half and correctly uses
 * seconds_B, which still holds the pre-rollover value.
 *
 * The 1/4 and 3/4 timing (rather than 0 and 1/2) provides margin: the seconds
 * counters are updated 1/4 second before they're needed, allowing for any ISR
 * latency.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/uart/uart.h"
#include "periph/usb_cdc/usb_cdc.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_tim.h"

#define NUM_CHANNELS  2
#define CLOCK_FREQ_HZ 170000000

#define TIMESTAMP_PRINT_PERIOD_USEC 100000
#define TIMESTAMP_BUFLEN            256  // should be power of 2 for fast modulo ops
#define MONOTONICITY_CHECK          0

#define LED_CLOCK GPIO_B7
#define LED_CHAN0 GPIO_A4
#define LED_CHAN1 GPIO_A5

// channel configurations
typedef struct {
  // number of missed pulses
  uint32_t num_missed;
  uint32_t buf_overflows;

  // divider state
  uint32_t divider;
  uint32_t count;

  // previously acquired timestamp
  uint32_t prev_seconds;
  uint32_t prev_counter;

  bool recent_pulse;
} channel_t;

static channel_t channels[NUM_CHANNELS];

// buffer of recorded timestamps that have not yet been transmitted over uart
typedef struct {
  uint32_t seconds;
  uint32_t counter;
  uint8_t channel;
  uint8_t _pad[7];  // pad to 16 bytes for power-of-2 indexing in ISR
} timestamp_t;

UartState_t uart;
static usbd_cdc_state_t usb_cdc;

// Circular buffer for timestamps: ISR writes at ts_head, USB drains from ts_tail
timestamp_t timestamp_buffer[TIMESTAMP_BUFLEN];
static volatile uint32_t ts_head = 0;  // next write position (ISR)
static volatile uint32_t ts_tail = 0;  // next read position (USB output)

// USB TX buffer - must persist until tx_complete callback
static char usb_tx_buf[50];

// total seconds elapsed since boot -- for details, see comment at top
uint32_t seconds_A = 0;
uint32_t seconds_B = 0;

CCMRAM static void missed_pulse(uint8_t channel_num) {
  channel_t *chan = &channels[channel_num];

  chan->num_missed++;
}

CCMRAM static void maybe_store_timestamp(uint8_t channel_num, uint32_t counter) {
  channel_t *chan = &channels[channel_num];

  // Get the high order bits
  const uint32_t seconds = counter < (CLOCK_FREQ_HZ / 2) ? seconds_A : seconds_B;

#if MONOTONICITY_CHECK
  // check for monotonicity
  if (chan->prev_seconds > seconds ||
      (chan->prev_seconds == seconds &&
       chan->prev_counter > counter)) {
    uart_print(&uart, ">>>> BUG: Nonmonotonic timestamps!\n");
    return;
  }
  chan->prev_seconds = seconds;
  chan->prev_counter = counter;
#endif

  // Skip divider logic entirely when divider == 1 (common case)
  if (__builtin_expect(chan->divider != 1, 0)) {
    chan->count++;
    if (chan->count < chan->divider) {
      return;
    }
    chan->count = 0;
  }

  // Cache ts_head locally - ISR is sole writer, so safe to read once
  uint32_t head = ts_head;
  uint32_t next_head = (head + 1) % TIMESTAMP_BUFLEN;
  if (__builtin_expect(next_head == ts_tail, 0)) {
    // buffer full
    chan->buf_overflows++;
    return;
  }

  timestamp_buffer[head].channel = channel_num;
  timestamp_buffer[head].seconds = seconds;
  timestamp_buffer[head].counter = counter;
  ts_head = next_head;
}

// TIM15 fires twice per rollover of TIM2, the input capture timer. It is used
// to update the high order bits. For details, see comment at top.
CCMRAM void TIM1_BRK_TIM15_IRQHandler() {
  if (LL_TIM_IsActiveFlag_UPDATE(TIM15)) {
    LL_TIM_ClearFlag_UPDATE(TIM15);

    // Get the big counter value
    const uint32_t big_counter = TIM2->CNT;

    // If the big counter is in its second half, increment the first-half high order bits.
    // Otherwise, set the second-half high order bits equal to the first-half.
    if (big_counter > (CLOCK_FREQ_HZ / 2)) {
      seconds_A++;
    } else {
      seconds_B = seconds_A;
    }
  }
}


// TIM2 fires only when the timer captures input.
CCMRAM void TIM2_IRQHandler() {
  // Cache SR to determine which channels have pending captures.
  // In input capture mode, reading CCRx auto-clears CCxIF in hardware
  // (RM0440 §29.4.12), so no explicit flag clear is needed.
  const uint32_t sr = TIM2->SR;

  if (sr & TIM_SR_CC1IF) {
    maybe_store_timestamp(0, TIM2->CCR1);
  }
  if (sr & TIM_SR_CC2IF) {
    maybe_store_timestamp(1, TIM2->CCR2);
  }

  // Re-read SR for overcapture detection instead of using the cached value.
  // A new capture can arrive between the initial SR read and the CCRx read,
  // setting CCxOF (overcapture) without being visible in the cached SR. Since
  // CCxOF doesn't generate its own interrupt, a missed OVR flag would linger
  // undetected until the next capture event.
  //
  // After the CCRx reads above, CCxIF is cleared, so any pulse arriving after
  // this re-read sets CCxIF fresh (not overcapture) and triggers ISR re-entry
  // normally -- there is no equivalent race after this point.
  const uint32_t sr_ovr = TIM2->SR;
  if (__builtin_expect(sr_ovr & (TIM_SR_CC1OF | TIM_SR_CC2OF), 0)) {
    if (sr_ovr & TIM_SR_CC1OF) {
      LL_TIM_ClearFlag_CC1OVR(TIM2);
      missed_pulse(0);
    }
    if (sr_ovr & TIM_SR_CC2OF) {
      LL_TIM_ClearFlag_CC2OVR(TIM2);
      missed_pulse(1);
    }
  }
}

static void print_one_timestamp(timestamp_t *t) {
  // Conversion from ticks to picoseconds:
  //
  // The clock frequency is 170 mhz, and we have the timer/counter configured to
  // roll over once per second, meaning a full second has 170,000,000 ticks.
  // There are 1T picoseconds in a second, so about 5,882 picoseconds per
  // tick. We can achieve this by multiplying by 100,000 and then dividing by
  // 17. 170M * 100,000 is more than 32 bits, so we use 64-bit math for the
  // conversion.
  uint64_t picoseconds = t->counter;
  picoseconds *= 100000;
  picoseconds /= 17;

  // Now we have picoseconds in a 64-bit int. Alas, nanolibc can not print 64
  // bit integers, so we'll break it into the high 6 digits (microseconds) and
  // the low 6 digits (picoseconds minus microseconds).
  uint32_t microseconds = picoseconds / 1000000;
  uint32_t sub_microseconds = picoseconds % 1000000;

  int len = snprintf(usb_tx_buf, sizeof(usb_tx_buf), "%d %ld.%06ld%06ld\n",
                     t->channel + 1, t->seconds, microseconds, sub_microseconds);
  usbd_cdc_write(&usb_cdc, usb_tx_buf, len);
}

static bool received_recent_pulse(uint8_t channel_num) {
  if (channels[channel_num].recent_pulse) {
    channels[channel_num].recent_pulse = false;
    return true;
  }

  return false;
}

static void update_leds(void) {
  static bool on_phase = false;

  on_phase = !on_phase;

  if (on_phase) {
    // blink the clock LED unconditionally
    gpio_set(LED_CLOCK);

    // blink the channel LEDs only if they've received recent pulses
    if (received_recent_pulse(0)) {
        gpio_set(LED_CHAN0);
    }
    if (received_recent_pulse(1)) {
      gpio_set(LED_CHAN1);
    }
  } else {
    gpio_clr(LED_CLOCK);
    gpio_clr(LED_CHAN0);
    gpio_clr(LED_CHAN1);
  }
}

static void try_send_next_timestamp(void) {
  if (ts_tail != ts_head && usbd_cdc_tx_ready(&usb_cdc)) {
    timestamp_t t = timestamp_buffer[ts_tail];
    ts_tail = (ts_tail + 1) % TIMESTAMP_BUFLEN;
    channels[t.channel].recent_pulse = true;  // for LED blinking
    print_one_timestamp(&t);
  }
}

static void usb_tx_complete(usbd_cdc_state_t *cdc, void *user_data) {
  try_send_next_timestamp();
}

static void periodic_task(void *data) {
  schedule_us(TIMESTAMP_PRINT_PERIOD_USEC, periodic_task, NULL);

  // Check for mid-stream clock failure detected by CSS via the generic
  // NMI handler in stm32-hardware.c
  if (g_rulos_hse_failed) {
    LL_TIM_DisableCounter(TIM2);
    LL_TIM_DisableCounter(TIM15);
    LL_TIM_DisableIT_CC1(TIM2);
    LL_TIM_DisableIT_CC2(TIM2);
    LL_TIM_DisableIT_UPDATE(TIM15);

    // quick flash showing clock failure
    static bool on = false;
    on = !on;
    gpio_set_or_clr(LED_CLOCK, on);
    gpio_set_or_clr(LED_CHAN0, on);
    gpio_set_or_clr(LED_CHAN1, on);

    static bool error_printed = false;
    if (!error_printed && usbd_cdc_tx_ready(&usb_cdc)) {
      error_printed = true;
      usbd_cdc_print(&usb_cdc, "# FATAL: External oscillator failure. Connect a 10MHz source and press reset.\n");
    }
    return;
  }

  // If USB has just come up for the first time, print a welcome message
  static bool welcome_printed = false;
  if (!welcome_printed && usbd_cdc_tx_ready(&usb_cdc)) {
     usbd_cdc_print(&usb_cdc, "# Starting timestamper, version " STRINGIFY(GIT_COMMIT) "\n");
     welcome_printed = true;
  }

  update_leds();

  // Report missed pulses when USB is idle
  if (ts_tail == ts_head && usbd_cdc_tx_ready(&usb_cdc)) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
      uint32_t missed = channels[i].num_missed;
      uint32_t overflows = channels[i].buf_overflows;
      if (missed || overflows) {
        channels[i].num_missed = 0;
        channels[i].buf_overflows = 0;
        int len = snprintf(usb_tx_buf, sizeof(usb_tx_buf),
                           "# ch%d: %ld overcaptures, %ld buf overflows\n",
                           i + 1, missed, overflows);
        usbd_cdc_write(&usb_cdc, usb_tx_buf, len);
        return;  // one message per period; USB will drain the rest
      }
    }
  }

  try_send_next_timestamp();
}

static void init_timers() {
  // Start the TIM2 clock
  __HAL_RCC_TIM2_CLK_ENABLE();

  // Configure the timer to roll over and generate an interrupt once per second,
  // i.e. the autoreload value is equal to the clock frequency in hz (minus 1).
  HAL_NVIC_SetPriority(TIM2_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
  LL_TIM_InitTypeDef timer2_init = {
    .Prescaler = 0,
    .CounterMode = LL_TIM_COUNTERMODE_UP,
    .Autoreload = CLOCK_FREQ_HZ - 1,
    .ClockDivision = LL_TIM_CLOCKDIVISION_DIV1,
    .RepetitionCounter = 0,
  };
  LL_TIM_Init(TIM2, &timer2_init);
  LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_UPDATE);
  LL_TIM_DisableMasterSlaveMode(TIM2);

  // Configure port A, pin 0 GPIO to "alternate function 1" mode, which on the
  // G4 is TIM2, channel 1. On the 32-pin QFP package of the STM32G431, PA0 is
  // on pin 5.
  LL_GPIO_InitTypeDef gpio_init = {0};
  gpio_init.Pin = LL_GPIO_PIN_0;
  gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
  gpio_init.Speed = LL_GPIO_SPEED_FREQ_LOW;
  gpio_init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  gpio_init.Pull = LL_GPIO_PULL_NO;
  gpio_init.Alternate = LL_GPIO_AF_1;
  LL_GPIO_Init(GPIOA, &gpio_init);

  // Configure input capture to be the rising edge of timer 2, channel 1
  LL_TIM_IC_SetActiveInput(TIM2, LL_TIM_CHANNEL_CH1,
                           LL_TIM_ACTIVEINPUT_DIRECTTI);
  LL_TIM_IC_SetPrescaler(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_ICPSC_DIV1);
  LL_TIM_IC_SetFilter(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_IC_FILTER_FDIV1);
  LL_TIM_IC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_IC_POLARITY_RISING);
  LL_TIM_EnableIT_CC1(TIM2);

  // Configure input capture to be the rising edge of timer 2, channel 2
  gpio_init.Pin = LL_GPIO_PIN_1;
  LL_GPIO_Init(GPIOA, &gpio_init);
  LL_TIM_IC_SetActiveInput(TIM2, LL_TIM_CHANNEL_CH2,
                           LL_TIM_ACTIVEINPUT_DIRECTTI);
  LL_TIM_IC_SetPrescaler(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_ICPSC_DIV1);
  LL_TIM_IC_SetFilter(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_IC_FILTER_FDIV1);
  LL_TIM_IC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_IC_POLARITY_RISING);
  LL_TIM_EnableIT_CC2(TIM2);

  /// Start the counter running and enable the input capture channels
  LL_TIM_EnableCounter(TIM2);
  LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH1);
  LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH2);


  // Configure TIM15: fires twice per TIM2 rollover to increment the high bits
  // of the counter.
  __HAL_RCC_TIM15_CLK_ENABLE();
  HAL_NVIC_SetPriority(TIM1_BRK_TIM15_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(TIM1_BRK_TIM15_IRQn);
  #define SMALLCOUNTER_MAX 10000
  LL_TIM_InitTypeDef timer15_init = {
    // The main counter counts to the clock frequency (170MHz), but TIM15 is
    // only a 16-bit counter which can't count that high. We scale everything
    // down so it counts up to 10,000 in the time it takes the main counter
    // counts to 170M.
    .Prescaler = (CLOCK_FREQ_HZ / SMALLCOUNTER_MAX) - 1,
    .CounterMode = LL_TIM_COUNTERMODE_UP,

    // Set the autoreload to half the 16B count so this timer fires twice per
    // rollover of the main counter.
    .Autoreload = (SMALLCOUNTER_MAX/2) - 1,
    .ClockDivision = LL_TIM_CLOCKDIVISION_DIV1,
    .RepetitionCounter = 0,
  };
  LL_TIM_Init(TIM15, &timer15_init);
  LL_TIM_DisableMasterSlaveMode(TIM15);
  LL_TIM_ClearFlag_UPDATE(TIM15);
  LL_TIM_EnableIT_UPDATE(TIM15);

  // Start the counter one-quarter of the way through the full-counter rollover
  // period so that this one fires 1/4 and 3/4 of the way through the main
  // counter.
  LL_TIM_SetCounter(TIM15, SMALLCOUNTER_MAX/4);
  LL_TIM_EnableCounter(TIM15);
}

int main() {
  rulos_hal_init();

  // initialize scheduler with 10msec jiffy clock
  init_clock(10000, TIMER1);

  // initialize channel configs. TODO: Add command-line interface so divider
  // values can be configured dynamically
  memset(&channels, 0, sizeof(channels));
  for (int i = 0; i < NUM_CHANNELS; i++) {
    channels[i].divider = 1;
  }

  // initialize uart for debug logging
  uart_init(&uart, /*uart_id=*/0, 1000000);
  log_bind_uart(&uart);

  // initialize the output LEDs
  gpio_make_output(LED_CLOCK);
  gpio_make_output(LED_CHAN0);
  gpio_make_output(LED_CHAN1);

  // initialize USB CDC for timestamp output
  usb_cdc = (usbd_cdc_state_t){
      .rx_cb = NULL,
      .tx_complete_cb = usb_tx_complete,
      .user_data = NULL,
  };
  usbd_cdc_init(&usb_cdc);

  // initialize the main timer and its input capture pin
  init_timers();

  schedule_us(1, periodic_task, NULL);
  scheduler_run();
}
