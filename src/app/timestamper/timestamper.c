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
 * serial interface. It has a resolution of 4 nanoseconds.
 *
 * It is based on the STM32H523, a Cortex-M33 running at 250 MHz with four
 * input-capture channels on TIM2. A 10 MHz active oscillator (rubidium or
 * GPS-disciplined) is expected on OSC_IN via HSE bypass mode; the PLL
 * multiplies to 250 MHz SYSCLK. If HSE fails to start, rulos_hal_init
 * falls back to HSI (~1% accuracy) and sets g_rulos_hse_failed -- the
 * periodic task below detects this and refuses to emit timestamps,
 * since HSI accuracy is meaningless for measurement-grade work.
 *
 * Output is simply a list of channel numbers and timestamps, one per line:
 *
 * 0 5293.585203496
 *
 * Timestamps are decimal seconds, with 9 digits following the decimal point
 * representing nanoseconds. Timestamps are relative to the time the program
 * started.
 *
 * The implementation uses the input-capture feature of TIM2, a 32-bit timer of
 * the STM32H523. Input capture waits for the rising edge of the input signal
 * and then latches the timer value at the moment of the signal's edge. The
 * timer runs at the system clock rate of 250 MHz. The timer is configured to
 * roll over once per second, i.e. every 250 million clock ticks.
 *
 * RACE CONDITION AVOIDANCE WITH seconds_A / seconds_B:
 *
 * A timestamp consists of two parts: the high-order "seconds" counter
 * (maintained in software) and the low-order timer value (latched in hardware
 * at the moment of input capture). A naive implementation would increment the
 * seconds counter every time we get an interrupt indicating the 250MHz counter
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
#include "periph/scpi/scpi.h"
#include "periph/uart/uart.h"
#include "periph/usb_cdc/usb_cdc.h"
#include "scpi.h"
#include "stm32h5xx_ll_tim.h"
#include "timestamper.h"

#ifndef RULOS_USE_HSE
#error "timestamper requires RULOS_USE_HSE"
#endif

// NUM_CHANNELS comes from timestamper.h, shared with scpi.c.
#define CLOCK_FREQ_HZ 250000000

// 250 MHz exactly = 4 ns per tick. Used by format_timestamp to convert
// counter ticks to nanoseconds; static_assert below guards against silent
// breakage if CLOCK_FREQ_HZ changes.
#define NS_PER_TICK 4
_Static_assert(CLOCK_FREQ_HZ == 1000000000 / NS_PER_TICK,
               "format_timestamp's tick->ns conversion assumes 4 ns/tick");

#define TIMESTAMP_PRINT_PERIOD_USEC 100000
// H523 has 256 KB SRAM. Sized to use ~192 KB for capture buffers,
// leaving ~64 KB for stack, USB buffers, and globals.
#define TIMESTAMP_BUFLEN            16384  // 16384 * 8 bytes = 128 KB; power of 2 for fast modulo
#define DMA_CAPTURE_BUFLEN          4096   // 4 channels * 4096 * 4 bytes = 64 KB
// USB FS bulk IN can transfer up to 19 packets (19*64 = 1216 bytes)
// per 1ms frame. 1280 bytes gives headroom to fill a full frame
// without cutting off mid-timestamp.
#define USB_TX_BUFLEN               1280
#define NUM_TX_BUFS                 2
#define MONOTONICITY_CHECK          0

// Channel number is packed into the top 2 bits of the counter field.
// Counter maxes out at 250,000,000 (0x0EE6_B280), well within 28 bits.
// 2 bits encode exactly 4 channels (0..3) -- no headroom needed since
// NUM_CHANNELS is at the hardware limit of TIM2's input-capture channels.
#define COUNTER_CHAN_SHIFT 30
#define COUNTER_CHAN_MASK  0x3FFFFFFFU

// PH1 is freed by HSE bypass (the otherwise-OSC_OUT pin on H5, same
// trick as the G4 board freeing PF1).
#define LED_CHAN0 GPIO_H1  // channel 0
#define LED_CHAN1 GPIO_A4  // channel 1
#define LED_CHAN2 GPIO_A5  // channel 2
#define LED_CHAN3 GPIO_A6  // channel 3
#define LED_CLOCK GPIO_B5  // 10 MHz HSE health
#define LED_USB   GPIO_A7  // USB state

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

  // Input-capture slope. Default RISING at boot; configurable via SCPI
  // INPut<n>:SLOPe.
  timestamper_slope_t slope;

  // divider state -- report only every Nth pulse. divider == 1 reports
  // every pulse. Configurable via SCPI INPut<n>:DIVider.
  uint32_t divider;
  uint32_t count;

  // previously acquired timestamp
  uint32_t prev_seconds;
  uint32_t prev_counter;

  volatile bool recent_pulse;
} channel_t;

static channel_t channels[NUM_CHANNELS];

// Continuous timestamp output enable (settable via SCPI OUTPut:STATe).
// Default ON so a freshly-plugged-in device behaves as the user expects.
static bool stream_enabled = true;
static timestamper_format_t format_mode = TIMESTAMPER_FORMAT_TEXT;

// Set by timestamper_discard_pending(); cleared in try_send_timestamps
// once the marker has been written to USB. The marker emission has to
// bypass stream_enabled because discard_pending always silences the
// stream around the clear.
static volatile bool marker_pending = false;
static const char marker_msg[] = "# output discarded\n";

// PA0..PA3 carry TIM2_CH1..CH4 input-capture signals. The TIM2 channel
// codes are also used at runtime by the slope-setter, hence file scope.
static const struct {
  uint32_t pin;         // LL_GPIO_PIN_n
  uint32_t ll_channel;  // LL_TIM_CHANNEL_CHn
} cap_pins[NUM_CHANNELS] = {
    {LL_GPIO_PIN_0, LL_TIM_CHANNEL_CH1},
    {LL_GPIO_PIN_1, LL_TIM_CHANNEL_CH2},
    {LL_GPIO_PIN_2, LL_TIM_CHANNEL_CH3},
    {LL_GPIO_PIN_3, LL_TIM_CHANNEL_CH4},
};

// total seconds elapsed since boot -- for details, see comment at top.
// volatile because TIM15 ISR writes these and process_dma_captures
// reads them from both DMA ISR and main-thread (flush) context.
volatile uint32_t seconds_A = 0;
volatile uint32_t seconds_B = 0;

// Circular buffer for timestamps not yet written to USB
typedef struct {
  uint32_t seconds;
  uint32_t counter;  // top 2 bits = channel, bottom 30 = counter value
} timestamp_t;

timestamp_t timestamp_buffer[TIMESTAMP_BUFLEN];
static volatile uint32_t ts_head = 0;  // next write position (ISR)
static volatile uint32_t ts_tail = 0;  // next read position (USB output)

// Output devices. The USB CDC connection is owned by the SCPI library
// (src/lib/periph/scpi); this app shares the same connection for
// streaming timestamps using scpi_usb_cdc_handle().
static UartState_t uart;

// Ping-pong USB TX buffers. While USB is sending one buffer, the
// periodic task fills the other with formatted timestamps. When the
// TX complete callback fires, the pre-filled buffer is handed to USB
// immediately — no formatting delay, so we never miss a USB frame.
static char usb_tx_bufs[NUM_TX_BUFS][USB_TX_BUFLEN];
static int tx_filling = 0;   // index of buffer currently being filled
static int tx_fill_pos = 0;  // bytes written into the filling buffer


// TIM15 fires twice per rollover of TIM2, the input capture timer. It is used
// to update the high order bits. For details, see comment at top.
CCMRAM void TIM15_IRQHandler() {
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
// Branches on format_mode set by SCPI.
//
// TEXT: at 250 MHz, each tick is 4 ns. Counter ranges 0..249,999,999;
// counter * 4 yields nanoseconds in 0..999,999,996, within uint32_t.
// 9 fractional digits give nanosecond resolution.
//
// BINARY: 8 bytes -- the on-device timestamp_t layout (4 bytes seconds
// LE, then 4 bytes counter LE with channel in the top 2 bits) is the
// wire format. memcpy straight out, skipping the 9-digit divmod chain
// in snprintf entirely.
static int format_timestamp(timestamp_t *t, char *buf, int buf_size) {
  if (format_mode == TIMESTAMPER_FORMAT_BINARY) {
    _Static_assert(sizeof(timestamp_t) == TIMESTAMPER_BINARY_RECORD_LEN,
                   "binary wire format mirrors timestamp_t layout");
    if (buf_size < (int)sizeof(timestamp_t)) return 0;
    memcpy(buf, t, sizeof(timestamp_t));
    return sizeof(timestamp_t);
  }
  uint8_t channel = t->counter >> COUNTER_CHAN_SHIFT;
  uint32_t counter = t->counter & COUNTER_CHAN_MASK;
  uint32_t nanoseconds = counter * NS_PER_TICK;
  return snprintf(buf, buf_size, "%d %lu.%09lu\n",
                  channel, t->seconds, nanoseconds);
}

static bool received_recent_pulse(uint8_t channel_num) {
  if (channels[channel_num].recent_pulse) {
    channels[channel_num].recent_pulse = false;
    return true;
  }

  return false;
}

// USB activity for the LED is tracked inside the SCPI library; we just
// drain its sticky bit each LED update via scpi_consume_recent_activity.

// Populated at startup by main(). Can't be a static const initializer
// because GPIO_An are themselves `static const gpio_pin_t` and so not
// constant expressions in the C sense.
static gpio_pin_t led_chan_pins[NUM_CHANNELS];

static void update_leds(void) {
  static bool on_phase = false;
  on_phase = !on_phase;

  // 10 MHz HSE health: a steady heartbeat regardless of input activity.
  // Off when HSE has failed.
  if (g_rulos_hse_failed) {
    gpio_clr(LED_CLOCK);
  } else {
    gpio_set_or_clr(LED_CLOCK, on_phase);
  }

  // Each activity LED has an idle state and a "blink" state opposite to
  // idle. The blink shows up during whichever phase is opposite the
  // idle level: on_phase for channels (idle off), off_phase for USB
  // (idle on). During the other phase the LED rests at idle.
  //
  // Continuous activity sets the LED's "blink" state every cycle, so
  // both lights flash 5 Hz in phase with the clock LED. A single pulse
  // produces one visible 100 ms blink the next time the LED's active
  // phase comes around.

  for (int i = 0; i < NUM_CHANNELS; i++) {
    gpio_set_or_clr(led_chan_pins[i],
                    on_phase && received_recent_pulse(i));
  }

  const bool usb_blink_off = !on_phase && scpi_consume_recent_activity();
  gpio_set_or_clr(LED_USB, scpi_usb_ready() && !usb_blink_off);
}

// Format timestamps from the ring buffer into the current filling
// buffer until it's full or the ring is empty. Called eagerly from
// periodic_task so the buffer is pre-filled by the time USB finishes
// the previous send.
static void fill_tx_buf(void) {
  while (ts_tail != ts_head) {
    // Max formatted length: "4 4294967295.999999996\n" = 23 bytes.
    if (tx_fill_pos + 23 > USB_TX_BUFLEN) {
      break;
    }
    timestamp_t t = timestamp_buffer[ts_tail];
    ts_tail = (ts_tail + 1) % TIMESTAMP_BUFLEN;
    tx_fill_pos += format_timestamp(
        &t, usb_tx_bufs[tx_filling] + tx_fill_pos,
        USB_TX_BUFLEN - tx_fill_pos);
  }
}

// If the filling buffer has data and USB is idle, hand it off and
// swap to the other buffer. Then eagerly start filling the new
// buffer so it's ready when USB finishes.
static void try_send_timestamps(void) {
  // ABOR's marker bypasses stream_enabled so the host can synchronize
  // even after silencing the stream during a discard_pending() dance.
  if (marker_pending && usbd_cdc_tx_ready(scpi_usb_cdc_handle())) {
    marker_pending = false;
    usbd_cdc_write(scpi_usb_cdc_handle(), (uint8_t *)marker_msg,
                   sizeof(marker_msg) - 1);
    return;
  }
  if (!stream_enabled) return;
  if (tx_fill_pos == 0 || !usbd_cdc_tx_ready(scpi_usb_cdc_handle())) {
    return;
  }

  usbd_cdc_write(scpi_usb_cdc_handle(), usb_tx_bufs[tx_filling], tx_fill_pos);

  // Swap: the buffer we just handed to USB is now "sending" (owned
  // by the USB stack). Start filling the other one.
  tx_filling = 1 - tx_filling;
  tx_fill_pos = 0;
  fill_tx_buf();
}

// Forwarded by the SCPI library after each USB CDC TX completes. Drives
// the ping-pong handoff: as soon as the in-flight buffer drains, we
// pre-arm the freshly-filled one.
static void on_usb_tx_complete(void) {
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

  // HSE failed at boot or mid-stream (the latter detected by CSS via
  // the NMI handler in stm32-hardware.c). HSI fallback can't produce
  // measurement-accurate timestamps, so refuse to operate and surface
  // the error to the user. Per LED policy, LED_CLOCK is now off
  // (handled by update_leds), and we flash the channel LEDs in unison
  // as a "this device is broken" indicator.
  if (g_rulos_hse_failed) {
    LL_TIM_DisableCounter(TIM2);
    LL_TIM_DisableCounter(TIM15);
    for (int i = 0; i < NUM_CHANNELS; i++) {
      rulos_dma_stop(channels[i].dma_ch);
    }
    LL_TIM_DisableIT_UPDATE(TIM15);

    static bool on = false;
    on = !on;
    for (int i = 0; i < NUM_CHANNELS; i++) {
      gpio_set_or_clr(led_chan_pins[i], on);
    }

    static bool error_printed = false;
    if (!error_printed && usbd_cdc_tx_ready(scpi_usb_cdc_handle())) {
      error_printed = true;
      usbd_cdc_print(scpi_usb_cdc_handle(), "# FATAL: External oscillator failure. Connect a 10MHz source and press reset.\n");
    }
    return;
  }

  // If USB has just come up for the first time, print a welcome message
  static bool welcome_printed = false;
  if (!welcome_printed && format_mode == TIMESTAMPER_FORMAT_TEXT &&
      usbd_cdc_tx_ready(scpi_usb_cdc_handle())) {
     usbd_cdc_print(scpi_usb_cdc_handle(), "# Starting LectroTIC-4, version " STRINGIFY(GIT_COMMIT) "\n");
     welcome_printed = true;
  }

  flush_dma_captures();
  update_leds();

  // Report missed pulses when USB is idle and no timestamps pending.
  // Use the filling buffer for one-off messages since nothing else is
  // writing to it in this path. Suppressed while streaming is disabled
  // so SCPI query responses aren't interleaved with overflow noise.
  if (stream_enabled && format_mode == TIMESTAMPER_FORMAT_TEXT &&
      ts_tail == ts_head && tx_fill_pos == 0 &&
      usbd_cdc_tx_ready(scpi_usb_cdc_handle())) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
      uint32_t missed = channels[i].num_missed;
      uint32_t overflows = channels[i].buf_overflows;
      if (missed || overflows) {
        channels[i].num_missed = 0;
        channels[i].buf_overflows = 0;
        int len = snprintf(usb_tx_bufs[tx_filling], USB_TX_BUFLEN,
                           "# ch%d: %lu overcaptures, %lu buf overflows\n",
                           i, missed, overflows);
        usbd_cdc_write(scpi_usb_cdc_handle(), usb_tx_bufs[tx_filling], len);
        tx_filling = 1 - tx_filling;
        return;  // one message per period; USB will drain the rest
      }
    }
  }

  fill_tx_buf();
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

  // PA0..PA3 carry TIM2_CH1..CH4 input-capture signals on AF1 (same
  // alternate-function mapping on G4 and H5). Mode must be ALTERNATE
  // so the AF mux routes the pin to TIM2; OutputType / Speed don't
  // apply since the pin is sensed, not driven.
  LL_GPIO_InitTypeDef gpio_init = {0};
  gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
  gpio_init.Pull = LL_GPIO_PULL_DOWN;
  gpio_init.Alternate = LL_GPIO_AF_1;

  // cap_pins is at file scope so the slope-setter API can also reference
  // each channel's TIM2 channel code at runtime.
  for (int i = 0; i < NUM_CHANNELS; i++) {
    gpio_init.Pin = cap_pins[i].pin;
    LL_GPIO_Init(GPIOA, &gpio_init);
    LL_TIM_IC_SetActiveInput(TIM2, cap_pins[i].ll_channel,
                             LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetPrescaler(TIM2, cap_pins[i].ll_channel, LL_TIM_ICPSC_DIV1);
    LL_TIM_IC_SetFilter(TIM2, cap_pins[i].ll_channel, LL_TIM_IC_FILTER_FDIV1);
    timestamper_set_slope(i, channels[i].slope);
  }

  /// Start the counter running and enable the input capture channels
  LL_TIM_EnableCounter(TIM2);
  for (int i = 0; i < NUM_CHANNELS; i++) {
    LL_TIM_CC_EnableChannel(TIM2, cap_pins[i].ll_channel);
  }

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
      {RULOS_DMA_REQ_TIM2_CH3, &TIM2->CCR3, TIM_DIER_CC3DE},
      {RULOS_DMA_REQ_TIM2_CH4, &TIM2->CCR4, TIM_DIER_CC4DE},
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
  HAL_NVIC_SetPriority(TIM15_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(TIM15_IRQn);
  #define SMALLCOUNTER_MAX 10000
  LL_TIM_InitTypeDef timer15_init = {
    // The main counter counts to the clock frequency (250MHz), but TIM15 is
    // only a 16-bit counter which can't count that high. We scale everything
    // down so it counts up to 10,000 in the time it takes the main counter
    // counts to 250M.
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

// ---------------------------------------------------------------------------
// Public API for the SCPI dispatcher in scpi.c. Declared in timestamper.h.

// IEEE 488.2 *IDN? response: <vendor>,<model>,<serial>,<firmware>.
// Serial is a placeholder for now; the chip UID could replace it once
// Get_SerialNum's output is plumbed up here.
const char *const timestamper_idn =
    "Lectrobox,LectroTIC-4,0," STRINGIFY(GIT_COMMIT);

void timestamper_set_slope(int ch, timestamper_slope_t slope) {
  if (ch < 0 || ch >= NUM_CHANNELS) return;
  uint32_t pol;
  switch (slope) {
    case TIMESTAMPER_SLOPE_FALLING: pol = LL_TIM_IC_POLARITY_FALLING; break;
    case TIMESTAMPER_SLOPE_BOTH:    pol = LL_TIM_IC_POLARITY_BOTHEDGE; break;
    case TIMESTAMPER_SLOPE_RISING:
    default:                        pol = LL_TIM_IC_POLARITY_RISING; break;
  }
  channels[ch].slope = slope;
  LL_TIM_IC_SetPolarity(TIM2, cap_pins[ch].ll_channel, pol);
}

timestamper_slope_t timestamper_get_slope(int ch) {
  if (ch < 0 || ch >= NUM_CHANNELS) return TIMESTAMPER_SLOPE_RISING;
  return channels[ch].slope;
}

void timestamper_set_divider(int ch, uint32_t n) {
  if (ch < 0 || ch >= NUM_CHANNELS || n == 0) return;
  // Reset count first so the next pulse starts a fresh group at the new
  // divider; the DMA ISR reads divider/count atomically enough on M33
  // (single-word loads) that a brief in-flight overlap is harmless.
  channels[ch].count = 0;
  channels[ch].divider = n;
}

uint32_t timestamper_get_divider(int ch) {
  if (ch < 0 || ch >= NUM_CHANNELS) return 1;
  return channels[ch].divider;
}

void timestamper_set_stream_enabled(bool enabled) {
  stream_enabled = enabled;
}

bool timestamper_get_stream_enabled(void) {
  return stream_enabled;
}

void timestamper_set_format(timestamper_format_t fmt) {
  format_mode = fmt;
}

timestamper_format_t timestamper_get_format(void) {
  return format_mode;
}

void timestamper_reset_all(void) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    timestamper_set_slope(i, TIMESTAMPER_SLOPE_RISING);
    timestamper_set_divider(i, 1);
  }
  timestamper_set_stream_enabled(true);
  timestamper_set_format(TIMESTAMPER_FORMAT_TEXT);
  timestamper_discard_pending();
}

void timestamper_discard_pending(void) {
  // ts_head is touched by the DMA ISR and the periodic flush; pause
  // interrupts briefly so we don't race a half-written entry. The
  // in-flight USB TX half also gets reset so any partially-formatted
  // record is dropped. The marker flag is set inside the same critical
  // section so it can't be observed before the ring is empty.
  __disable_irq();
  ts_head = 0;
  ts_tail = 0;
  tx_fill_pos = 0;
  for (int i = 0; i < NUM_CHANNELS; i++) {
    channels[i].num_missed = 0;
    channels[i].buf_overflows = 0;
  }
  marker_pending = true;
  __enable_irq();
}

int main() {
  rulos_hal_init();

  // initialize scheduler with 10msec jiffy clock
  init_clock(10000, TIMER1);

  // Initial channel state. Slope and divider are runtime-configurable
  // via SCPI; defaults match what *RST also restores.
  memset(&channels, 0, sizeof(channels));
  for (int i = 0; i < NUM_CHANNELS; i++) {
    channels[i].divider = 1;
    channels[i].slope = TIMESTAMPER_SLOPE_RISING;
  }

  // initialize uart for debug logging
  uart_init(&uart, /*uart_id=*/0, 1000000);
  log_bind_uart(&uart);

  // initialize the output LEDs
  led_chan_pins[0] = LED_CHAN0;
  led_chan_pins[1] = LED_CHAN1;
  led_chan_pins[2] = LED_CHAN2;
  led_chan_pins[3] = LED_CHAN3;
  gpio_make_output(LED_CLOCK);
  gpio_make_output(LED_USB);
  for (int i = 0; i < NUM_CHANNELS; i++) {
    gpio_make_output(led_chan_pins[i]);
  }

  // Initialise the SCPI front-end. This also brings up the USB CDC
  // connection used both for incoming SCPI commands and for outgoing
  // timestamp streams.
  timestamper_scpi_init(on_usb_tx_complete);

  // initialize the main timer and its input capture pin
  init_timers();

  schedule_us(1, periodic_task, NULL);
  scheduler_run();
}
