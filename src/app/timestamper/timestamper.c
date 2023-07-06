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
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/uart/uart.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_tim.h"

#define NUM_CHANNELS  2
#define CLOCK_FREQ_HZ 170000000

#define TIMESTAMP_PRINT_PERIOD_USEC 100000
#define TIMESTAMP_BUFLEN            200
#define MONOTONICITY_CHECK          0

// channel configurations
typedef struct {
  // number of missed pulses
  uint32_t num_pulses;
  uint32_t num_missed;
  uint32_t buf_overflows;

  // divider state
  uint32_t divider;
  uint32_t count;

  // previously acquired timestamp
  uint32_t prev_seconds;
  uint32_t prev_counter;
} channel_t;

static channel_t channels[NUM_CHANNELS];

// buffer of recorded timestamps that have not yet been transmitted over uart
typedef struct {
  uint32_t seconds;
  uint32_t counter;
  uint8_t channel;
} timestamp_t;

UartState_t uart;
int num_timestamps = 0;
timestamp_t timestamp_buffer[TIMESTAMP_BUFLEN];

// total seconds elapsed since boot -- for details, see comment at top
uint32_t seconds_A = 0;
uint32_t seconds_B = 0;

static void missed_pulse(uint8_t channel_num) {
  channel_t *chan = &channels[channel_num];

  chan->num_missed++;
}

static void maybe_store_timestamp(uint8_t channel_num, uint32_t counter) {
  channel_t *chan = &channels[channel_num];
  chan->num_pulses++;

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
#endif

  chan->prev_seconds = seconds;
  chan->prev_counter = counter;

  chan->count++;
  if (chan->count < chan->divider) {
    return;
  }

  // divider count has been reached: reset the counter and store the timestamp
  chan->count = 0;

  if (num_timestamps >= TIMESTAMP_BUFLEN) {
    chan->buf_overflows++;
    return;
  }

  timestamp_buffer[num_timestamps].channel = channel_num;
  timestamp_buffer[num_timestamps].seconds = seconds;
  timestamp_buffer[num_timestamps].counter = counter;
  num_timestamps++;
}

// TIM15 fires twice per rollover of TIM2, the input capture timer. It is used
// to update the high order bits. For details, see comment at top.
void TIM1_BRK_TIM15_IRQHandler() {
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
void TIM2_IRQHandler() {
  // Channel 1
  if (LL_TIM_IsActiveFlag_CC1(TIM2)) {
    // Timer channel 1 has captured an input signal.
    LL_TIM_ClearFlag_CC1(TIM2);
    maybe_store_timestamp(0, TIM2->CCR1);
  } else if (LL_TIM_IsActiveFlag_CC1OVR(TIM2)) {
    LL_TIM_ClearFlag_CC1OVR(TIM2);
    missed_pulse(0);
  }

  // Channel 2
  else if (LL_TIM_IsActiveFlag_CC2(TIM2)) {
    // Timer channel 2 has captured an input signal.
    LL_TIM_ClearFlag_CC2(TIM2);
    maybe_store_timestamp(1, TIM2->CCR2);
  } else if (LL_TIM_IsActiveFlag_CC2OVR(TIM2)) {
    missed_pulse(1);
    LL_TIM_ClearFlag_CC2OVR(TIM2);

  } else {
    // unexpected interrupt
    volatile int tim2sr = TIM2->SR;
    char buf[100];
    sprintf(buf, "got unexpected interrupt, timer2 status register=0x%x",
            tim2sr);
    uart_print(&uart, buf);
    __builtin_trap();
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

  char buf[50];
  int len = snprintf(buf, sizeof(buf), "%d %ld.%06ld%06ld\n", t->channel + 1,
                     t->seconds, microseconds, sub_microseconds);
  uart_write(&uart, buf, len);
}

static void drain_output_buffer(void *data) {
  schedule_us(TIMESTAMP_PRINT_PERIOD_USEC, drain_output_buffer, NULL);

  // In critical section, copy the entire timestamp buffer into a temporary area
  // and immediately release the lock. This minimizes the time we might miss a
  // timestamp.
  //
  // TODO: avoid copies by switching back and forth between two buffers, one of
  // which is filling with new timestamps and one of which is draining to uart.
  timestamp_t tmpbuf[TIMESTAMP_BUFLEN];
  int num_copied;

  // start of critical section
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  memcpy(tmpbuf, timestamp_buffer, num_timestamps * sizeof(timestamp_t));
  num_copied = num_timestamps;
  num_timestamps = 0;
  hal_end_atomic(old_interrupts);
  // end of critical section

  // print any timestamps we have have copied out of the buffer
  for (int i = 0; i < num_copied; i++) {
    print_one_timestamp(&tmpbuf[i]);
  }
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

  // initialize uart
  uart_init(&uart, /*uart_id=*/0, 1000000);
  uart_print(&uart,
             "# Starting timestamper, version " STRINGIFY(GIT_COMMIT) "\n");

  schedule_us(1, drain_output_buffer, NULL);

  // initialize the main timer and its input capture pin
  init_timers();

  scheduler_run();
}
