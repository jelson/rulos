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
 * This program timestamps pulses that it sees on its input capture pins and
 * prints a list of those timestamps (relative to program start) to its USB
 * serial interface. It has a resolution of 6 nanoseconds.
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
 * Output is simply a list of channel numbers and timestamps, one per line:
 *
 * 1 5293.585203495384
 *
 * Timestamps are decimal seconds, with 12 digits following the decimal point
 * representing picoseconds. Timestamps are relative to the time the program
 * started.
 *
 * The implementation uses input-capture feature of TIM2, a 32-bit timer of the
 * STM32G431. Input capture waits for the rising edge of the input signal and
 * then latches the timer value at the moment of the signal's edge. The timer
 * runs at the system clock rate of 170MHz. The 170MHz clock is achieved by
 * using the STM32's PLL to multiply the 10MHz reference frequency by 17. The
 * timer is configured to roll over once per second, i.e. every 170 million
 * clock ticks.
 *
 * RACE CONDITION AVOIDANCE WITH seconds_A / seconds_B:
 *
 * A timestamp consists of two parts: the high-order "seconds" counter
 * (maintained in software) and the low-order timer value (latched in hardware
 * at the moment of input capture). A naive implementation would increment the
 * seconds counter every time we get an interrupt indicating the 170MHz counter
 * has rolled over. But there's a potential race condition: if the timer rolls
 * over between when an input capture occurs and when the ISR reads the seconds
 * counter, the wrong seconds value could be paired with the timer value.
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
 * - If counter < half: use seconds_A (event was in the first half of the
 *   second)
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

#include "core/dma.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/uart/uart.h"
#include "periph/usb_cdc/usb_cdc.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_tim.h"

#define NUM_CHANNELS  2
#define CLOCK_FREQ_HZ 170000000

#define TIMESTAMP_PRINT_PERIOD_USEC 100000
#define TIMESTAMP_BUFLEN            1024  // should be power of 2 for fast modulo ops
#define DMA_CAPTURE_BUFLEN          256
#define USB_TX_BUFLEN               1024
#define MONOTONICITY_CHECK          0

// Channel number is packed into the top 2 bits of the counter field.
// Counter maxes out at 170,000,000 (0x0A21FE80), well within 30 bits.
#define COUNTER_CHAN_SHIFT 30
#define COUNTER_CHAN_MASK  0x3FFFFFFFU

#define LED_CLOCK GPIO_F1
#define LED_CHAN0 GPIO_A4
#define LED_CHAN1 GPIO_A5

// Per-channel state: statistics, divider, previous timestamp, and DMA.
// Passed to the DMA callbacks via user_data so a single pair of
// callback functions serves all channels.
typedef struct {
  // DMA circular buffer and progress cursor
  volatile uint32_t dma_buf[DMA_CAPTURE_BUFLEN];
  uint32_t dma_processed_pos;
  rulos_dma_channel_t *dma_ch;

  // channel identity (index into this array)
  uint8_t channel_num;

  // statistics
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

// total seconds elapsed since boot -- for details, see comment at top
uint32_t seconds_A = 0;
uint32_t seconds_B = 0;

// Circular buffer for timestamps not yet written to USB
typedef struct {
  uint32_t seconds;
  uint32_t counter;  // top 2 bits = channel, bottom 30 = counter value
} timestamp_t;

timestamp_t timestamp_buffer[TIMESTAMP_BUFLEN];
static volatile uint32_t ts_head = 0;  // next write position (ISR)
static volatile uint32_t ts_tail = 0;  // next read position (USB output)

// Output device state. USB packets carry at most 64 bytes of payload, but we
// want the buffer to be big enough that the USB stack can complete multiple
// transactions per frame, maximizing throughput.
static UartState_t uart;
static usbd_cdc_state_t usb_cdc;
static char usb_tx_buf[USB_TX_BUFLEN];


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


// Process a batch of raw DMA-captured counter values into timestamp_buffer.
// Called from DMA ISR and periodic flush — ts_head/ts_tail can't change
// during execution, so we cache them locally to avoid volatile penalties.
CCMRAM static void process_dma_captures(uint8_t channel_num,
                                  volatile uint32_t *buf,
                                  uint32_t offset, uint32_t count) {
  channel_t *chan = &channels[channel_num];
  chan->recent_pulse = true;
  uint32_t head = ts_head;
  const uint32_t tail = ts_tail;
  const uint32_t chan_tag = (uint32_t)channel_num << COUNTER_CHAN_SHIFT;
  const uint32_t divider = chan->divider;

  // Pre-check that the timestamp buffer has room for the entire batch.
  // If not enough space, store what we can and count the drops.
  uint32_t available = (tail - head - 1) % TIMESTAMP_BUFLEN;
  if (__builtin_expect(available < count, 0)) {
    chan->buf_overflows += count - available;
    count = available;
    if (count == 0) return;
  }

  // Use pointer-based iteration so the compiler doesn't spill count
  volatile uint32_t *src = buf + offset;
  volatile uint32_t *const end = src + count;

  while (src < end) {
    uint32_t counter = *src++;
    uint32_t seconds = counter < (CLOCK_FREQ_HZ / 2) ? seconds_A : seconds_B;

    if (__builtin_expect(divider != 1, 0)) {
      chan->count++;
      if (chan->count < divider) continue;
      chan->count = 0;
    }

    timestamp_buffer[head].seconds = seconds;
    timestamp_buffer[head].counter = counter | chan_tag;
    head = (head + 1) % TIMESTAMP_BUFLEN;
  }

  ts_head = head;
}

// DMA HT/TC callbacks for TIM2 input capture. HT fires when the first
// half of the circular buffer is full; TC fires when the second half
// is full (i.e. at each buffer wrap). The core DMA layer has already
// cleared the hardware flag before calling us.
//
// Each callback starts from dma_processed_pos rather than a fixed
// offset, so it doesn't reprocess entries the periodic flush already
// handled.
CCMRAM static void on_dma_ht(void *user_data) {
  channel_t *ch = (channel_t *)user_data;
  const uint32_t half = DMA_CAPTURE_BUFLEN / 2;
  uint32_t last = ch->dma_processed_pos;
  if (last < half) {
    process_dma_captures(ch->channel_num, ch->dma_buf, last, half - last);
    ch->dma_processed_pos = half;
  }
}

CCMRAM static void on_dma_tc(void *user_data) {
  channel_t *ch = (channel_t *)user_data;
  uint32_t last = ch->dma_processed_pos;
  if (last >= DMA_CAPTURE_BUFLEN / 2) {
    process_dma_captures(ch->channel_num, ch->dma_buf, last,
                         DMA_CAPTURE_BUFLEN - last);
  }
  ch->dma_processed_pos = 0;
}

// Format a timestamp into buf, returning the number of bytes written.
static int format_timestamp(timestamp_t *t, char *buf, int buf_size) {
  // Conversion from ticks to picoseconds:
  //
  // The clock frequency is 170 mhz, and we have the timer/counter configured to
  // roll over once per second, meaning a full second has 170,000,000 ticks.
  // There are 1T picoseconds in a second, so about 5,882 picoseconds per
  // tick. We can achieve this by multiplying by 100,000 and then dividing by
  // 17. 170M * 100,000 is more than 32 bits, so we use 64-bit math for the
  // conversion.
  uint8_t channel = t->counter >> COUNTER_CHAN_SHIFT;
  uint32_t counter = t->counter & COUNTER_CHAN_MASK;

  uint64_t picoseconds = counter;
  picoseconds *= 100000;
  picoseconds /= 17;

  // Now we have picoseconds in a 64-bit int. Alas, nanolibc can not print 64
  // bit integers, so we'll break it into the high 6 digits (microseconds) and
  // the low 6 digits (picoseconds minus microseconds).
  uint32_t microseconds = picoseconds / 1000000;
  uint32_t sub_microseconds = picoseconds % 1000000;

  return snprintf(buf, buf_size, "%d %ld.%06ld%06ld\n",
                  channel + 1, t->seconds, microseconds, sub_microseconds);
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

static void try_send_timestamps(void) {
  if (ts_tail == ts_head || !usbd_cdc_tx_ready(&usb_cdc)) {
    return;
  }
  // Pack as many timestamps as fit into the USB TX buffer. The HAL
  // handles splitting into 64-byte USB packets automatically, sending
  // multiple packets per 1ms USB frame for ~16x higher throughput.
  int pos = 0;
  while (ts_tail != ts_head) {
    // Max formatted length: "2 4294967295.999999999999\n" = 26 bytes.
    // Stop if there's not enough room for the largest possible timestamp.
    if (pos + 26 > USB_TX_BUFLEN) {
      break;
    }
    timestamp_t t = timestamp_buffer[ts_tail];
    ts_tail = (ts_tail + 1) % TIMESTAMP_BUFLEN;
    pos += format_timestamp(&t, usb_tx_buf + pos, USB_TX_BUFLEN - pos);
  }
  usbd_cdc_write(&usb_cdc, usb_tx_buf, pos);
}

static void usb_tx_complete(usbd_cdc_state_t *cdc, void *user_data) {
  try_send_timestamps();
}

// Flush any DMA-captured values that haven't reached a half-buffer boundary
// yet. This ensures slow pulses get processed promptly rather than waiting for
// DMA_CAPTURE_BUFLEN/2 more captures to fill the half-buffer. Called from
// periodic_task.
static void flush_dma_captures(void) {
  const uint32_t half = DMA_CAPTURE_BUFLEN / 2;

  for (int i = 0; i < NUM_CHANNELS; i++) {
    channel_t *ch = &channels[i];

    // Disable interrupts to coordinate with the DMA HT/TC ISR — both
    // read and update the same processed_pos, so we must not race.
    __disable_irq();

    // DMA counts DOWN from DMA_CAPTURE_BUFLEN; current write position is
    // DMA_CAPTURE_BUFLEN minus the remaining count.
    uint32_t remaining = rulos_dma_get_remaining(ch->dma_ch);
    uint32_t current_pos = DMA_CAPTURE_BUFLEN - remaining;
    if (current_pos == DMA_CAPTURE_BUFLEN) {
      current_pos = 0;  // DMA counter reads 0 right at wrap
    }

    // Only process within the current half — never cross a half-boundary.
    // The DMA HT/TC ISR handles boundary transitions. Clamp current_pos
    // to the next boundary so we don't step on the ISR's territory.
    uint32_t last = ch->dma_processed_pos;
    uint32_t limit = (last < half) ? half : DMA_CAPTURE_BUFLEN;
    if (current_pos > limit) {
      current_pos = limit;
    }

    if (current_pos > last) {
      process_dma_captures(ch->channel_num, ch->dma_buf, last,
                           current_pos - last);
      ch->dma_processed_pos = current_pos;
    }

    __enable_irq();
  }
}

static void periodic_task(void *data) {
  schedule_us(TIMESTAMP_PRINT_PERIOD_USEC, periodic_task, NULL);

  // Check for mid-stream clock failure detected by CSS via the generic
  // NMI handler in stm32-hardware.c
  if (g_rulos_hse_failed) {
    LL_TIM_DisableCounter(TIM2);
    LL_TIM_DisableCounter(TIM15);
    for (int i = 0; i < NUM_CHANNELS; i++) {
      rulos_dma_stop(channels[i].dma_ch);
    }
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

  flush_dma_captures();
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

  try_send_timestamps();
}

static void init_timers() {
  // Start the TIM2 clock
  __HAL_RCC_TIM2_CLK_ENABLE();

  // Configure the timer to roll over and generate an interrupt once per second,
  // i.e. the autoreload value is equal to the clock frequency in hz (minus 1).
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

  // Configure input capture to be the rising edge of timer 2, channel 2
  gpio_init.Pin = LL_GPIO_PIN_1;
  LL_GPIO_Init(GPIOA, &gpio_init);
  LL_TIM_IC_SetActiveInput(TIM2, LL_TIM_CHANNEL_CH2,
                           LL_TIM_ACTIVEINPUT_DIRECTTI);
  LL_TIM_IC_SetPrescaler(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_ICPSC_DIV1);
  LL_TIM_IC_SetFilter(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_IC_FILTER_FDIV1);
  LL_TIM_IC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_IC_POLARITY_RISING);

  /// Start the counter running and enable the input capture channels
  LL_TIM_EnableCounter(TIM2);
  LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH1);
  LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH2);

  // Set up DMA for input capture. Each capture channel gets its own DMA
  // channel in circular mode via the RULOS DMA abstraction. HT/TC
  // callbacks fire at each half-buffer boundary so we can process
  // captures while the other half fills.
  static const struct {
    rulos_dma_request_t request;
    volatile uint32_t *ccr;
    uint32_t ll_dma_req_flag;  // LL_TIM_EnableDMAReq_CCn macro arg
  } dma_setup[NUM_CHANNELS] = {
      {RULOS_DMA_REQ_TIM2_CH1, &TIM2->CCR1, TIM_DIER_CC1DE},
      {RULOS_DMA_REQ_TIM2_CH2, &TIM2->CCR2, TIM_DIER_CC2DE},
  };

  for (int i = 0; i < NUM_CHANNELS; i++) {
    channels[i].channel_num = i;
    const rulos_dma_config_t cfg = {
        .request = dma_setup[i].request,
        .direction = RULOS_DMA_DIR_PERIPH_TO_MEM,
        .mode = RULOS_DMA_MODE_CIRCULAR,
        .periph_width = RULOS_DMA_WIDTH_WORD,
        .mem_width = RULOS_DMA_WIDTH_WORD,
        .periph_increment = false,
        .mem_increment = true,
        .priority = RULOS_DMA_PRIORITY_VERYHIGH,
        .tc_callback = on_dma_tc,
        .ht_callback = on_dma_ht,
        .user_data = &channels[i],
    };
    channels[i].dma_ch = rulos_dma_alloc(&cfg);
    if (channels[i].dma_ch == NULL) {
      __builtin_trap();
    }
    rulos_dma_start(channels[i].dma_ch, (volatile void *)dma_setup[i].ccr,
                    (void *)channels[i].dma_buf, DMA_CAPTURE_BUFLEN);
    SET_BIT(TIM2->DIER, dma_setup[i].ll_dma_req_flag);
  }


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
