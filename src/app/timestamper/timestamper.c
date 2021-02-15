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
 * timestamps, one per line. Each is in decimal seconds, with 9 digits following
 * the decimal point representing nanoseconds. Timestamps are relative to the
 * time the program started.
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
 * timer scale of one tick per 170-millionths of a second to nanoseconds, by
 * multiplying it by 100 and then dividing it by 17. (64-bit math is used for
 * this to prevent overflow.)
 */

#include <string.h>

#include "core/hardware.h"
#include "core/rulos.h"

#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_tim.h"

#define CLOCK_FREQ_HZ 170000000
#define ONEHZ_DEBUG_PIN GPIO_B5

#define TEST_INPUT_PIN GPIO_B4
#define TEST_INPUT_PERIOD_USEC 1000000

#define TIMESTAMP_PRINT_PERIOD_USEC 100000
#define TIMESTAMP_BUFLEN 200


// circular buffer of recorded timestamps that have not yet been transmitted over uart
typedef struct {
  uint32_t seconds;
  uint32_t nanoseconds;
} timestamp_t;

int num_timestamps = 0;
timestamp_t timestamp_buffer[TIMESTAMP_BUFLEN];
bool buffer_overflow = false;

// total seconds elapsed since boot
uint32_t seconds;

// uart handle for transmitting timestamps
HalUart uart;

void store_timestamp(uint32_t seconds, uint32_t nanoseconds) {
  if (num_timestamps >= TIMESTAMP_BUFLEN) {
    buffer_overflow = true;
    return;
  }

  timestamp_buffer[num_timestamps].seconds = seconds;
  timestamp_buffer[num_timestamps].nanoseconds = nanoseconds;
  num_timestamps++;
}

// Timer callback that fires at the update rate (~10khz).
void TIM2_IRQHandler() {
  if (LL_TIM_IsActiveFlag_UPDATE(TIM2)) {
    LL_TIM_ClearFlag_UPDATE(TIM2);

    gpio_set(ONEHZ_DEBUG_PIN);
    seconds++;
    gpio_clr(ONEHZ_DEBUG_PIN);
  } else if (LL_TIM_IsActiveFlag_CC1(TIM2)) {
    LL_TIM_ClearFlag_CC1(TIM2);

    // timestamp conversion from ticks to nanoseconds:
    //
    // The clock frequency is 170 mhz, and we have the timer/counter configured
    // to roll over once per second, meaning a full second has 170,000,000
    // ticks. There are 1B nanoseconds in a second, so about 6 nanoseconds per
    // tick. We can achieve this by multiplying by 100 and then dividing by
    // 17. 170M * 100 is more than 32 bits, so we use 64-bit math for the
    // conversion.
    uint64_t nanoseconds = TIM2->CCR1;
    nanoseconds *= 100;
    nanoseconds /= 17;
    store_timestamp(seconds, nanoseconds);
  } else {
    // unexpected interrupt
    __builtin_trap();
  }
}

void init_timer() {
  // Initialize the timer
  __HAL_RCC_TIM2_CLK_ENABLE();
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

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

  // configure input capture GPIO pin
  LL_GPIO_InitTypeDef gpio_init = {0};
  gpio_init.Pin = LL_GPIO_PIN_0;
  gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
  gpio_init.Speed = LL_GPIO_SPEED_FREQ_LOW;
  gpio_init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  gpio_init.Pull = LL_GPIO_PULL_NO;
  gpio_init.Alternate = LL_GPIO_AF_1;
  LL_GPIO_Init(GPIOA, &gpio_init);

  // turn on input capture for timer 2, channel 1 -- pin PA0 (pin 5)
  LL_TIM_IC_SetActiveInput(TIM2, LL_TIM_CHANNEL_CH1,
                           LL_TIM_ACTIVEINPUT_DIRECTTI);
  LL_TIM_IC_SetPrescaler(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_ICPSC_DIV1);
  LL_TIM_IC_SetFilter(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_IC_FILTER_FDIV1);
  LL_TIM_IC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_IC_POLARITY_RISING);
  LL_TIM_EnableIT_CC1(TIM2);

  LL_TIM_EnableCounter(TIM2);
  LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH1);

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

static void drain_output_buffer(void *data) {
  schedule_us(TIMESTAMP_PRINT_PERIOD_USEC, drain_output_buffer, NULL);

  while (true) {
    timestamp_t output;
    bool has_output = false;

    // in critical section, check to see if there's a timestamp to emit
    rulos_irq_state_t old_interrupts = hal_start_atomic();

    if (num_timestamps > 0) {
      output = timestamp_buffer[0];
      num_timestamps--;
      memmove(&timestamp_buffer[0], &timestamp_buffer[1], num_timestamps * sizeof(timestamp_t));
      has_output = true;
    }
    hal_end_atomic(old_interrupts);
    // end of critical section

    // if we got a timestamp, emit it to the serial port
    if (has_output)  {
      char buf[50];
      snprintf(buf, sizeof(buf), "%ld.%09ld\n", output.seconds, output.nanoseconds);
      hal_uart_sync_send(&uart, buf);
    } else {
      return;
    }
  }
}


int main() {
  hal_init();

  // initialize scheduler with 10msec jiffy clock
  init_clock(10000, TIMER1);

  // set test pins to be outputs
  gpio_make_output(ONEHZ_DEBUG_PIN);
  gpio_clr(ONEHZ_DEBUG_PIN);

  gpio_make_output(TEST_INPUT_PIN);
  gpio_clr(TEST_INPUT_PIN);

  // initialize the main timer and its input capture pin
  init_timer();

  // initialize uart
  hal_uart_init(&uart, 1000000, true, /* uart_id= */ 0);

  schedule_us(1, create_test_input, NULL);
  schedule_us(1, drain_output_buffer, NULL);
  cpumon_main_loop();
}
