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

#define CLOCK_FREQ_HZ 170000000

#define TEST_INPUT_PIN         GPIO_B4
#define TEST_INPUT_PERIOD_USEC 1000000

#define TIMESTAMP_PRINT_PERIOD_USEC 100000
#define TIMESTAMP_BUFLEN            200

// buffer of recorded timestamps that have not yet been transmitted over uart
typedef struct {
  uint32_t seconds;
  uint32_t counter;
  uint8_t channel;
} timestamp_t;

UartState_t uart;
int num_timestamps = 0;
timestamp_t timestamp_buffer[TIMESTAMP_BUFLEN];
bool buffer_overflow = false;

// total seconds elapsed since boot
uint32_t seconds;

void store_timestamp(uint8_t channel, uint32_t seconds, uint32_t counter) {
  if (num_timestamps >= TIMESTAMP_BUFLEN) {
    buffer_overflow = true;
    return;
  }

  timestamp_buffer[num_timestamps].channel = channel;
  timestamp_buffer[num_timestamps].seconds = seconds;
  timestamp_buffer[num_timestamps].counter = counter;
  num_timestamps++;
}

static int chan2_divider = 0;
#define CHAN2_DIV_VALUE 10000000

// Timer interrupt that fires under two conditions:
// 1) When the timer rolls over, once per second.
// 2) When the timer captures input
void TIM2_IRQHandler() {
  if (LL_TIM_IsActiveFlag_UPDATE(TIM2)) {
    LL_TIM_ClearFlag_UPDATE(TIM2);

    // Timer has rolled over. Happens at 1hz, we just update the seconds
    // counter.
    seconds++;
  }

  // Channel 1
  else if (LL_TIM_IsActiveFlag_CC1(TIM2)) {
    // Timer channel 1 has captured an input signal.
    LL_TIM_ClearFlag_CC1(TIM2);
    store_timestamp(1, seconds, TIM2->CCR1);
  } else if (LL_TIM_IsActiveFlag_CC1OVR(TIM2)) {
    // overflow on capture. should we remember?
    LL_TIM_ClearFlag_CC1OVR(TIM2);

  }

  // Channel 2
  else if (LL_TIM_IsActiveFlag_CC2(TIM2)) {
    // Timer channel 2 has captured an input signal.
    LL_TIM_ClearFlag_CC2(TIM2);

    chan2_divider++;
    if (chan2_divider == CHAN2_DIV_VALUE) {
      store_timestamp(1, seconds, TIM2->CCR2);
      chan2_divider = 0;
    }
  } else if (LL_TIM_IsActiveFlag_CC2OVR(TIM2)) {
    // overflow on capture. should we remember?
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
  int len = snprintf(buf, sizeof(buf), "%d %ld.%06ld%06ld\n", t->channel,
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

void init_timer() {
  // Start the timer's clock
  __HAL_RCC_TIM2_CLK_ENABLE();

  // Configure the timer to roll over and generate an interrupt once per second,
  // i.e. the autoreload value is equal to the clock frequency in hz (minus 1).
  HAL_NVIC_SetPriority(TIM2_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
  LL_TIM_InitTypeDef timer_init;
  timer_init.Prescaler = 0;
  timer_init.CounterMode = LL_TIM_COUNTERMODE_UP;
  timer_init.Autoreload = CLOCK_FREQ_HZ - 1;
  timer_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  timer_init.RepetitionCounter = 0;
  LL_TIM_Init(TIM2, &timer_init);
  LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_UPDATE);
  LL_TIM_DisableMasterSlaveMode(TIM2);
  LL_TIM_EnableIT_UPDATE(TIM2);
  LL_TIM_GenerateEvent_UPDATE(TIM2);

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

  // reset the seconds part of our clock - should go after timer enablement
  // because turning on interrupts seems to generate a spurious one
  seconds = 0;
}

static void create_test_input(void *data) {
  schedule_us(TEST_INPUT_PERIOD_USEC, create_test_input, NULL);

  gpio_set(TEST_INPUT_PIN);
  for (volatile int i = 0; i < 4; i++) {
    asm("nop");
  }
  gpio_clr(TEST_INPUT_PIN);
}

int main() {
  rulos_hal_init();

  // initialize scheduler with 10msec jiffy clock
  init_clock(10000, TIMER1);

  // set test pins to be outputs
  gpio_make_output(TEST_INPUT_PIN);
  gpio_clr(TEST_INPUT_PIN);

  // initialize the main timer and its input capture pin
  init_timer();

  // initialize uart
  uart_init(&uart, /*uart_id=*/0, 1000000);
  uart_print(&uart, "# Starting timestamper\n");

  schedule_us(1, create_test_input, NULL);
  schedule_us(1, drain_output_buffer, NULL);
  scheduler_run();
}
