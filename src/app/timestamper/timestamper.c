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
 * Output is a list of channel numbers, timestamps, and edge polarities,
 * one captured edge per line:
 *
 * 0 5293.585203496 +
 *
 * Timestamps are decimal seconds, with 9 digits following the decimal point
 * representing nanoseconds, relative to the time the program started. The
 * trailing column is the edge polarity: '+' rising, '-' falling.
 *
 * EDGE POLARITY VIA PAIRED CAPTURE CHANNELS:
 *
 * TIM2's input capture latches only the counter, not the pin state, so
 * polarity can't be read after the fact. Instead each input feeds TWO
 * TIM2 capture channels off the same timer input: one armed rising on
 * the direct TI, one armed falling on the indirect TI (internally
 * sourced from the same TIx -- no second pin). Which channel captured
 * IS the polarity, latched in hardware at the moment of the edge. The
 * SCPI slope setting no longer re-arms the timer; it just selects
 * which of the two sub-streams are emitted. In BOTH mode the two
 * sub-streams are merged back into one time-ordered stream by
 * comparing raw tick values (see service_channel).
 *
 * The pairing halves the inputs per timer: TIM2's four capture
 * channels serve two polarity-capable inputs (PA0 = jack 0 and
 * PA2 = jack 2). Jacks 1 and 3 are inoperative in this build, pending
 * a second 32-bit timer (TIM5 on PA1/PA3) phase-locked to TIM2.
 *
 * Firmware update over USB DFU (no SWD probe needed; device not capturing):
 *
 *   sudo dfu-util -d 1209:71c4,0483:df11 -a 0 -s 0x08000000:leave -D <fw>.bin
 *
 * 1209:71c4 is the runtime VID:PID, 0483:df11 the ST ROM bootloader; the
 * two-tuple lets dfu-util find the running device, auto-detach it into the
 * bootloader, flash, and restart (:leave). If interrupted it stays in the
 * bootloader and the same command retries. Verify with `tsctl.py idn`.
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
#include "periph/nvconfig/nvconfig.h"
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

// counter field layout: [31:30] channel, [29] special-message flag,
// [28] edge polarity, [27:0] tick value. The tick value maxes at
// CLOCK_FREQ_HZ-1 = 249,999,999 (0x0EE6_B27F), which fits in 28 bits,
// so extracting it with the 28-bit value mask -- not a 30-bit
// "everything but channel" mask -- keeps the flag bits out of the
// nanoseconds.
#define COUNTER_CHAN_SHIFT 30
#define COUNTER_VALUE_MASK 0x0FFFFFFFU
_Static_assert(CLOCK_FREQ_HZ - 1 <= COUNTER_VALUE_MASK,
               "tick value must fit in the 28-bit counter value field");

// counter [28] = edge polarity of a timestamp record: 1 = rising
// (LOW->HIGH), 0 = falling. Hardware-latched, not inferred: each input
// feeds two permanently-armed capture channels (one rising, one
// falling), and the bit records which one captured. Special records
// ([29] set) keep [28] zero.
#define COUNTER_POLARITY_BIT (1u << 28)

// counter [29] = "special message" marker: the 8-byte record is not a
// timestamp but device metadata. When set, counter [7:0] is the
// message type and the seconds word carries its payload. [31:30] still
// names the channel the message concerns (0 when not channel-specific).
#define COUNTER_SPECIAL_BIT  (1u << 29)
#define COUNTER_MSGTYPE_MASK 0xFFu
#define MSG_OUTPUT_CLEARED 0u  // all-zero payload, still a full 8-byte
                               // record; binary analog of "# output
                               // cleared"
#define MSG_PULSES_LOST    1u  // payload: [15:0] overcaptures,
                               // [31:16] buf overflows (each sat. 0xFFFF)
#define MSG_OSC_FAIL       2u  // all-zero payload; reference-clock
                               // failure -- the device has halted
                               // (binary analog of the "# FATAL ..."
                               // oscillator line)

// PH1 is freed by HSE bypass (the otherwise-OSC_OUT pin on H5, same
// trick as the G4 board freeing PF1).
#define LED_CHAN0 GPIO_H1  // channel 0
#define LED_CHAN1 GPIO_A4  // channel 1
#define LED_CHAN2 GPIO_A5  // channel 2
#define LED_CHAN3 GPIO_A6  // channel 3
#define LED_CLOCK GPIO_B5  // 10 MHz HSE health
#define LED_USB   GPIO_A7  // USB state

// A channel's two hardware capture sub-streams.
#define SUB_RISING  0
#define SUB_FALLING 1
#define NUM_SUBS    2

// In-use TIM2 capture channels on this board: CH1..CH4, two per
// polarity-capable input. 64 KB total, same as the one-sub-per-input
// design.
#define NUM_HW_CAPTURE 4
static volatile uint32_t capture_buf[NUM_HW_CAPTURE][DMA_CAPTURE_BUFLEN];

// Per-sub holding FIFO for the BOTH-mode merge. The two subs capture
// the same alternating edge train 1:1, so their DMA write heads stay
// within O(1) of each other and the unpaired residual is tiny; this
// just covers ISR-timing jitter. Overflow is the overcapture/extreme
// regime -- counted (buf_overflows), never silent. Entries are
// resolved (seconds, ready-to-emit counter word) at stash time, which
// keeps every record's seconds_A/B resolution within one flush period
// of its capture -- well inside the rollover scheme's validity window.
#define STASH_LEN 64
typedef struct {
  uint32_t sec;
  uint32_t ctr;  // counter word: channel tag | polarity | ticks
} stash_rec_t;

// Per-channel state. A polarity-capable channel owns two TIM2 capture
// sub-streams off the same timer input (one armed rising, one armed
// falling); every record it emits carries channel_num and the
// hardware-latched polarity bit. On the current board only inputs
// PA0 and PA2 (jacks 0 and 2) have capture pairs -- TIM2's four
// capture channels make two pairs -- so channels 1 and 3 are
// inoperative until TIM5 picks up PA1/PA3.
typedef struct {
  struct {
    volatile uint32_t *buf;   // DMA circular buffer; NULL if no hw
    uint32_t merge_pos;       // next DMA index to drain (circular)
    uint32_t prev_ndtr;       // DMA remaining-count at last flush
                              // (quiescence detection)
    rulos_dma_channel_t *dma_ch;
    uint32_t polarity_bit;    // SUB_RISING: COUNTER_POLARITY_BIT; else 0
    stash_rec_t stash[STASH_LEN];
    uint16_t st_head;         // oldest stashed record
    uint16_t st_n;            // count held
  } sub[NUM_SUBS];
  bool has_hw;                // false for inoperative channels (1, 3)
  volatile bool dma_isr_ran;  // any capture HT/TC ISR since last flush
                              // (quiescence detection)

  // channel identity (index into this array)
  uint8_t channel_num;

  // statistics
  uint32_t num_missed;
  uint32_t buf_overflows;

  // Input-capture slope, configurable via SCPI INPut<n>:SLOPe. Both
  // edges are always captured in hardware; slope selects which
  // sub-streams are emitted.
  timestamper_slope_t slope;

  // divider state -- report only every Nth emitted edge. divider == 1
  // reports every edge. Configurable via SCPI INPut<n>:DIVider.
  uint32_t divider;
  uint32_t count;

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
static const char marker_msg[] = "# output cleared\n";

// Hardware-latched edge polarity: each polarity-capable input drives
// TWO TIM2 capture channels off the same timer input (TIx) -- one
// armed rising on the direct TI, one armed falling on the indirect TI
// (sourced internally from the same TIx, so no second pin). Edge
// direction is implicit in WHICH channel captured. Each capture
// channel keeps its own dedicated TIM2 DMA request, so raw capture is
// identical to the proven one-sub-per-input path.
//
// The four TIM2 capture channels, paired into the two polarity-capable
// inputs. gpio_pin == 0 means the channel is sourced from its pair's
// TI internally (indirect) and needs no pin. Channel numbers track the
// front-panel jack the pin is routed to: PA0 = jack 0, PA2 = jack 2.
//   CH1 -> channel 0, rising  (PA0 = TI1, direct)
//   CH2 -> channel 0, falling (TI1 indirect)
//   CH3 -> channel 2, rising  (PA2 = TI3, direct)
//   CH4 -> channel 2, falling (TI3 indirect)
// Channels 1 and 3 have no entry -- inoperative until TIM5 picks up
// PA1/PA3 (as TIM5 TI2/TI4) with the same pairing trick.
static const struct {
  uint32_t gpio_pin;      // LL_GPIO_PIN_n, or 0 for indirect (no pin)
  uint32_t ll_channel;    // LL_TIM_CHANNEL_CHn
  uint32_t active_input;  // LL_TIM_ACTIVEINPUT_DIRECTTI / _INDIRECTTI
  uint32_t ic_polarity;   // LL_TIM_IC_POLARITY_RISING / _FALLING
  uint8_t channel;        // owning channel
  uint8_t sub;            // SUB_RISING / SUB_FALLING
} capture_hw[NUM_HW_CAPTURE] = {
    {LL_GPIO_PIN_0, LL_TIM_CHANNEL_CH1, LL_TIM_ACTIVEINPUT_DIRECTTI,
     LL_TIM_IC_POLARITY_RISING, 0, SUB_RISING},
    {0, LL_TIM_CHANNEL_CH2, LL_TIM_ACTIVEINPUT_INDIRECTTI,
     LL_TIM_IC_POLARITY_FALLING, 0, SUB_FALLING},
    {LL_GPIO_PIN_2, LL_TIM_CHANNEL_CH3, LL_TIM_ACTIVEINPUT_DIRECTTI,
     LL_TIM_IC_POLARITY_RISING, 2, SUB_RISING},
    {0, LL_TIM_CHANNEL_CH4, LL_TIM_ACTIVEINPUT_INDIRECTTI,
     LL_TIM_IC_POLARITY_FALLING, 2, SUB_FALLING},
};

// total seconds elapsed since boot -- for details, see comment at top.
// volatile because TIM15 ISR writes these and resolve() reads them
// from both DMA ISR and main-thread (flush) context.
volatile uint32_t seconds_A = 0;
volatile uint32_t seconds_B = 0;

// Circular buffer for timestamps not yet written to USB
typedef struct {
  uint32_t seconds;
  uint32_t counter;  // [31:30] channel, [29] special-message flag,
                     // [28] edge polarity, [27:0] tick value /
                     // message type (see the COUNTER_* / MSG_*
                     // defines above)
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


// Resolve a raw TIM2 capture (timer ticks) into a ready-to-emit
// record: pick the seconds half from the tick value (top-of-file
// rollover scheme) and fold in the channel tag + polarity bit. The
// rollover scheme only tolerates a bounded capture-to-resolve delay
// (the A/B copies advance around the second boundaries), so this must
// run while the record is fresh -- the per-flush-period service
// cadence guarantees that.
CCMRAM static inline void resolve(channel_t *chan, uint8_t s,
                                  uint32_t raw, stash_rec_t *out) {
  out->sec = raw < (CLOCK_FREQ_HZ / 2) ? seconds_A : seconds_B;
  out->ctr = raw
      | ((uint32_t)chan->channel_num << COUNTER_CHAN_SHIFT)
      | chan->sub[s].polarity_bit;
}

// Push one resolved record to the timestamp ring, honoring the
// channel divider. A full ring is counted, never silent.
CCMRAM static inline void ring_emit(channel_t *chan,
                                    const stash_rec_t *r) {
  chan->recent_pulse = true;
  if (__builtin_expect(chan->divider != 1, 0)) {
    if (++chan->count < chan->divider) return;
    chan->count = 0;
  }
  uint32_t head = ts_head;
  if (__builtin_expect((ts_tail - head - 1) % TIMESTAMP_BUFLEN == 0, 0)) {
    chan->buf_overflows++;
    return;
  }
  timestamp_buffer[head].seconds = r->sec;
  timestamp_buffer[head].counter = r->ctr;
  ts_head = (head + 1) % TIMESTAMP_BUFLEN;
}

// True iff a is strictly earlier than b.
//
// Ordering is by raw timer tick modulo CLOCK_FREQ_HZ, NOT by the
// stashed seconds: `sec` comes from the seconds_A/seconds_B
// half-rollover scheme, which is only eventually-consistent (the
// TIM15 ISR updates it around the FREQ/2 and wrap boundaries). That
// is fine for formatting an already-ordered record, but using it for
// the merge's ordering decision lets two edges that straddle the
// boundary compare out of order.
//
// The two records the merge ever compares are the two subs' heads of
// the SAME alternating signal, always within ~one period of each
// other -- far under CLOCK_FREQ_HZ/2 -- so a half-window modular tick
// comparison gives the correct relative order across the second wrap
// with no dependence on the racy seconds resolution.
CCMRAM static inline bool rec_before(const stash_rec_t *a,
                                     const stash_rec_t *b) {
  uint32_t ta = a->ctr & COUNTER_VALUE_MASK;
  uint32_t tb = b->ctr & COUNTER_VALUE_MASK;
  int32_t d = (int32_t)tb - (int32_t)ta;
  if (d < 0) d += CLOCK_FREQ_HZ;  // wrap into [0, CLOCK_FREQ_HZ)
  return d != 0 && d < (int32_t)(CLOCK_FREQ_HZ / 2);
}

// DMA-safe end-of-region for sub `s`: records [merge_pos, cur) are
// captured and safe to read (cur is exclusive). Single-owner cursor:
// per-channel service is serialized by the same-priority DMA ISRs
// plus the irq-guarded flush.
CCMRAM static inline uint32_t safe_cur(channel_t *chan, uint8_t s) {
  uint32_t rem = rulos_dma_get_remaining(chan->sub[s].dma_ch);
  uint32_t cur = DMA_CAPTURE_BUFLEN - rem;
  return (cur == DMA_CAPTURE_BUFLEN) ? 0 : cur;
}

// A sub's next record comes from its stash (older, already resolved)
// first, then directly from its DMA buffer at merge_pos. peek leaves
// it in place; consume advances past it.
CCMRAM static inline bool sub_avail(channel_t *chan, uint8_t s,
                                    uint32_t cur) {
  return chan->sub[s].st_n > 0 || chan->sub[s].merge_pos != cur;
}
CCMRAM static inline void sub_peek(channel_t *chan, uint8_t s,
                                   stash_rec_t *out) {
  if (chan->sub[s].st_n > 0) {
    *out = chan->sub[s].stash[chan->sub[s].st_head];
  } else {
    resolve(chan, s, chan->sub[s].buf[chan->sub[s].merge_pos], out);
  }
}
CCMRAM static inline void sub_consume(channel_t *chan, uint8_t s) {
  if (chan->sub[s].st_n > 0) {
    chan->sub[s].st_head = (chan->sub[s].st_head + 1) % STASH_LEN;
    chan->sub[s].st_n--;
  } else {
    uint32_t p = chan->sub[s].merge_pos + 1;
    chan->sub[s].merge_pos = (p == DMA_CAPTURE_BUFLEN) ? 0 : p;
  }
}

// After the 2-way merge stalls (one sub ran out so ordering can't yet
// continue), the other sub's still-unconsumed DMA records can't stay
// in the circular DMA buffer -- the write head will lap them. Copy
// just that residual (the O(1) inter-sub skew, since both subs capture
// the same alternating edge train 1:1) into the stash and free the
// DMA region. Resolving here also bounds capture-to-resolve delay for
// records that stall across service calls. Overflow is the
// overcapture/extreme regime -- counted.
CCMRAM static void stash_residual(channel_t *chan, uint8_t s,
                                  uint32_t cur) {
  while (chan->sub[s].merge_pos != cur) {
    if (__builtin_expect(chan->sub[s].st_n >= STASH_LEN, 0)) {
      uint32_t dropped = (cur - chan->sub[s].merge_pos) % DMA_CAPTURE_BUFLEN;
      chan->buf_overflows += dropped;  // drop the rest (not silent)
      chan->sub[s].merge_pos = cur;
      return;
    }
    uint16_t w = (chan->sub[s].st_head + chan->sub[s].st_n) % STASH_LEN;
    resolve(chan, s, chan->sub[s].buf[chan->sub[s].merge_pos],
            &chan->sub[s].stash[w]);
    chan->sub[s].st_n++;
    uint32_t p = chan->sub[s].merge_pos + 1;
    chan->sub[s].merge_pos = (p == DMA_CAPTURE_BUFLEN) ? 0 : p;
  }
}

// Minimum age (in TIM2 ticks) of a record released by the drain path.
// Capture-to-DMA-visible latency is bounded by GPDMA arbitration
// (well under a microsecond), so any edge that could precede a
// record this old is already visible in its sub's DMA buffer -- the
// drain can never emit ahead of an edge it hasn't seen. In practice a
// drained straggler is at least one flush period (~100 ms) old, so
// the gate only ever bites if the quiescence detection is wrong.
#define DRAIN_MIN_AGE_TICKS (1000 / NS_PER_TICK)  // 1 us

// Service a channel.
//   slope != BOTH: only the selected sub streams -- one monotonic,
//     already-ordered stream copied straight to the ring (the proven
//     fast path, one resolve + one ring write per record); the other
//     sub is skipped past so its DMA buffer can't overflow.
//   slope == BOTH: 2-way merge by timestamp, reading directly from
//     both subs' DMA buffers (one resolve + compare + ring write per
//     record -- same per-record cost as the fast path). A record is
//     held until the other sub progresses past it (no earlier edge
//     can then still arrive); only the small post-merge residual is
//     copied to the stash. `drain` (channel verifiably quiescent for
//     a full flush period) releases the final straggler at burst end.
CCMRAM static void service_channel(channel_t *chan, bool drain) {
  timestamper_slope_t sl = chan->slope;

  if (sl != TIMESTAMPER_SLOPE_BOTH) {
    uint8_t act = (sl == TIMESTAMPER_SLOPE_FALLING) ? SUB_FALLING
                                                    : SUB_RISING;
    uint8_t oth = act ^ 1;
    // Stashed records (left over from a stint in BOTH mode) are older
    // than anything still in the DMA buffer -- emit them first.
    while (chan->sub[act].st_n > 0) {
      ring_emit(chan, &chan->sub[act].stash[chan->sub[act].st_head]);
      chan->sub[act].st_head = (chan->sub[act].st_head + 1) % STASH_LEN;
      chan->sub[act].st_n--;
    }
    volatile uint32_t *buf = chan->sub[act].buf;
    uint32_t cur = safe_cur(chan, act);
    uint32_t pos = chan->sub[act].merge_pos;
    while (pos != cur) {
      uint32_t seg_end = (pos < cur) ? cur : DMA_CAPTURE_BUFLEN;
      while (pos < seg_end) {
        stash_rec_t r;
        resolve(chan, act, buf[pos++], &r);
        ring_emit(chan, &r);
      }
      if (seg_end == DMA_CAPTURE_BUFLEN) pos = 0;
    }
    chan->sub[act].merge_pos = cur;
    chan->sub[act].st_head = chan->sub[act].st_n = 0;

    chan->sub[oth].merge_pos = safe_cur(chan, oth);
    chan->sub[oth].st_head = chan->sub[oth].st_n = 0;
    return;
  }

  uint32_t curR = safe_cur(chan, SUB_RISING);
  uint32_t curF = safe_cur(chan, SUB_FALLING);
  for (;;) {
    bool aR = sub_avail(chan, SUB_RISING, curR);
    bool aF = sub_avail(chan, SUB_FALLING, curF);
    uint8_t s;
    if (aR && aF) {
      stash_rec_t rR, rF;
      sub_peek(chan, SUB_RISING, &rR);
      sub_peek(chan, SUB_FALLING, &rF);
      s = rec_before(&rR, &rF) ? SUB_RISING : SUB_FALLING;
    } else if (drain && (aR || aF)) {
      s = aR ? SUB_RISING : SUB_FALLING;
      // Second, independent ordering guard (see DRAIN_MIN_AGE_TICKS):
      // never release a record so fresh that an earlier edge could
      // still be in flight to the other sub's DMA buffer.
      stash_rec_t r;
      sub_peek(chan, s, &r);
      int32_t age = (int32_t)TIM2->CNT
                    - (int32_t)(r.ctr & COUNTER_VALUE_MASK);
      if (age < 0) age += CLOCK_FREQ_HZ;
      if (age < DRAIN_MIN_AGE_TICKS) break;
    } else {
      break;
    }
    stash_rec_t r;
    sub_peek(chan, s, &r);
    ring_emit(chan, &r);
    sub_consume(chan, s);
  }
  stash_residual(chan, SUB_RISING, curR);
  stash_residual(chan, SUB_FALLING, curF);
}

// TIM2 capture DMA HT/TC callback (one per in-use sub). The NDTR-
// driven drain doesn't distinguish HT from TC, so both map here. All
// of a channel's sub DMA ISRs share one NVIC priority (cannot preempt
// each other), so per-channel service is serialized without masking.
CCMRAM static void on_dma(void *user_data) {
  channel_t *chan = (channel_t *)user_data;
  chan->dma_isr_ran = true;
  service_channel(chan, false);
}

// Format a timestamp into buf, returning the number of bytes written.
// Branches on format_mode set by SCPI.
//
// TEXT: at 250 MHz, each tick is 4 ns. Counter ranges 0..249,999,999;
// counter * 4 yields nanoseconds in 0..999,999,996, within uint32_t.
// 9 fractional digits give nanosecond resolution. The trailing column
// is the hardware-latched edge polarity: '+' rising, '-' falling.
//
// BINARY: 8 bytes -- the on-device timestamp_t layout (4 bytes seconds
// LE, then 4 bytes counter LE with channel in the top 2 bits and
// polarity in bit 28) is the wire format. memcpy straight out,
// skipping the 9-digit divmod chain in snprintf entirely.
static int format_timestamp(timestamp_t *t, char *buf, int buf_size) {
  if (format_mode == TIMESTAMPER_FORMAT_BINARY) {
    _Static_assert(sizeof(timestamp_t) == TIMESTAMPER_BINARY_RECORD_LEN,
                   "binary wire format mirrors timestamp_t layout");
    if (buf_size < (int)sizeof(timestamp_t)) return 0;
    memcpy(buf, t, sizeof(timestamp_t));
    return sizeof(timestamp_t);
  }
  uint8_t channel = t->counter >> COUNTER_CHAN_SHIFT;
  char polarity = (t->counter & COUNTER_POLARITY_BIT) ? '+' : '-';
  uint32_t counter = t->counter & COUNTER_VALUE_MASK;
  uint32_t nanoseconds = counter * NS_PER_TICK;
  return snprintf(buf, buf_size, "%d %lu.%09lu %c\n",
                  channel, t->seconds, nanoseconds, polarity);
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
    // Max formatted length: "3 4294967295.999999996 +\n" = 25 bytes.
    if (tx_fill_pos + 25 > USB_TX_BUFLEN) {
      break;
    }
    timestamp_t t = timestamp_buffer[ts_tail];
    ts_tail = (ts_tail + 1) % TIMESTAMP_BUFLEN;
    tx_fill_pos += format_timestamp(
        &t, usb_tx_bufs[tx_filling] + tx_fill_pos,
        USB_TX_BUFLEN - tx_fill_pos);
  }
}

// Emit one 8-byte special-message record on the binary stream. The
// buffer is static because usbd_cdc_write is asynchronous (DMA): it
// must outlive the transfer. Safe as a single instance because callers
// gate on USB idle and send at most one per service period, exactly
// like the text marker / diagnostic line.
static void send_special(uint8_t channel, uint8_t msgtype,
                         uint32_t payload) {
  static timestamp_t msg;
  msg.seconds = payload;
  msg.counter = ((uint32_t)channel << COUNTER_CHAN_SHIFT) |
                COUNTER_SPECIAL_BIT | msgtype;
  usbd_cdc_write(scpi_usb_cdc_handle(), (uint8_t *)&msg, sizeof(msg));
}

// If the filling buffer has data and USB is idle, hand it off and
// swap to the other buffer. Then eagerly start filling the new
// buffer so it's ready when USB finishes.
static void try_send_timestamps(void) {
  // The marker bypasses stream_enabled so the host can synchronize
  // even after silencing the stream during a discard_pending() dance.
  // Binary mode gets it as a special record, text mode as the comment.
  if (marker_pending && usbd_cdc_tx_ready(scpi_usb_cdc_handle())) {
    marker_pending = false;
    if (format_mode == TIMESTAMPER_FORMAT_BINARY) {
      send_special(0, MSG_OUTPUT_CLEARED, 0);
    } else {
      usbd_cdc_write(scpi_usb_cdc_handle(), (uint8_t *)marker_msg,
                     sizeof(marker_msg) - 1);
    }
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

// Periodic catch-up: drain/merge each channel promptly even when the
// DMA buffers fill slowly (the HT/TC ISR alone could otherwise leave a
// slow signal's captures waiting up to a half-buffer). Also releases
// the final BOTH-mode straggler once a channel is verifiably
// quiescent. Called from periodic_task.
//
// Quiescence must be EXACT: a false positive arms the drain path,
// which emits one sub without waiting for the other -- the only way
// the output can mis-order. A channel is quiescent iff no capture
// landed since the previous flush, established by two independent
// signals that cannot alias together:
//   1. per-sub DMA remaining-count unchanged since the last flush.
//      NDTR alone can alias -- it returns to the same value after
//      exactly DMA_CAPTURE_BUFLEN more captures -- but never sooner;
//   2. no capture HT/TC ISR ran since the last flush. Any
//      DMA_CAPTURE_BUFLEN-capture aliasing run necessarily crossed a
//      half-buffer boundary and fired one.
// (A summed or single-signal detector aliases under sustained
// high-rate input and was the root cause of a rare drain-path
// mis-order.)
static void flush_dma_captures(void) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    channel_t *ch = &channels[i];
    if (!ch->has_hw) continue;

    // Disable interrupts so we don't race the DMA ISR's call into
    // service_channel (shared merge_pos / stash / ring head).
    __disable_irq();

    uint32_t ndtr_r = rulos_dma_get_remaining(ch->sub[SUB_RISING].dma_ch);
    uint32_t ndtr_f = rulos_dma_get_remaining(ch->sub[SUB_FALLING].dma_ch);
    bool quiescent = !ch->dma_isr_ran &&
                     ndtr_r == ch->sub[SUB_RISING].prev_ndtr &&
                     ndtr_f == ch->sub[SUB_FALLING].prev_ndtr;
    ch->dma_isr_ran = false;
    ch->sub[SUB_RISING].prev_ndtr = ndtr_r;
    ch->sub[SUB_FALLING].prev_ndtr = ndtr_f;

    service_channel(ch, quiescent);

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
      if (!channels[i].has_hw) continue;
      for (int s = 0; s < NUM_SUBS; s++) {
        rulos_dma_stop(channels[i].sub[s].dma_ch);
      }
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
      if (format_mode == TIMESTAMPER_FORMAT_BINARY) {
        send_special(0, MSG_OSC_FAIL, 0);
      } else {
        usbd_cdc_print(scpi_usb_cdc_handle(), "# FATAL: External oscillator failure. Connect a 10MHz source and press reset.\n");
      }
    }
    return;
  }

  // If USB has just come up for the first time, print a welcome message
  static bool welcome_printed = false;
  if (!welcome_printed && format_mode == TIMESTAMPER_FORMAT_TEXT &&
      usbd_cdc_tx_ready(scpi_usb_cdc_handle())) {
     usbd_cdc_print(scpi_usb_cdc_handle(), "# Starting LectroTIC-4, version " TIMESTAMPER_FW_VERSION "-" STRINGIFY(GIT_COMMIT) "\n");
     welcome_printed = true;
  }

  flush_dma_captures();
  update_leds();

  // Report missed pulses when USB is idle and no timestamps pending:
  // a "# ch<N>: ..." line in text mode, a PULSES_LOST special record
  // in binary mode. Suppressed while streaming is disabled so SCPI
  // query responses aren't interleaved with overflow noise.
  if (stream_enabled && ts_tail == ts_head && tx_fill_pos == 0 &&
      usbd_cdc_tx_ready(scpi_usb_cdc_handle())) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
      uint32_t missed = channels[i].num_missed;
      uint32_t overflows = channels[i].buf_overflows;
      if (missed || overflows) {
        channels[i].num_missed = 0;
        channels[i].buf_overflows = 0;
        if (format_mode == TIMESTAMPER_FORMAT_BINARY) {
          uint32_t oc = missed > 0xFFFF ? 0xFFFF : missed;
          uint32_t bo = overflows > 0xFFFF ? 0xFFFF : overflows;
          send_special(i, MSG_PULSES_LOST, oc | (bo << 16));
        } else {
          // The filling buffer is free here -- nothing else writes it
          // in this path.
          int len = snprintf(usb_tx_bufs[tx_filling], USB_TX_BUFLEN,
                             "# ch%d: %lu overcaptures, %lu buf overflows\n",
                             i, missed, overflows);
          usbd_cdc_write(scpi_usb_cdc_handle(),
                         usb_tx_bufs[tx_filling], len);
          tx_filling = 1 - tx_filling;
        }
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

  // Each polarity-capable input's rising sub-stream uses a direct-TI
  // pin (PA0 = TI1 for channel 0, PA2 = TI3 for channel 2) configured
  // for AF input; its falling sub-stream is sourced from the same TI
  // internally (indirect, gpio_pin == 0) and needs no pin. AF1 is the
  // TIM2 IC mapping (same on G4 and H5); the pin is sensed, not
  // driven. Capture polarities are fixed in hardware -- the SCPI slope
  // selects which sub-streams are emitted, not how the timer is armed.
  LL_GPIO_InitTypeDef gpio_init = {0};
  gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
  gpio_init.Pull = LL_GPIO_PULL_DOWN;
  gpio_init.Alternate = LL_GPIO_AF_1;

  for (int k = 0; k < NUM_HW_CAPTURE; k++) {
    if (capture_hw[k].gpio_pin) {
      gpio_init.Pin = capture_hw[k].gpio_pin;
      LL_GPIO_Init(GPIOA, &gpio_init);
    }
    LL_TIM_IC_SetActiveInput(TIM2, capture_hw[k].ll_channel,
                             capture_hw[k].active_input);
    LL_TIM_IC_SetPrescaler(TIM2, capture_hw[k].ll_channel,
                           LL_TIM_ICPSC_DIV1);
    LL_TIM_IC_SetFilter(TIM2, capture_hw[k].ll_channel,
                        LL_TIM_IC_FILTER_FDIV1);
    LL_TIM_IC_SetPolarity(TIM2, capture_hw[k].ll_channel,
                          capture_hw[k].ic_polarity);
  }

  /// Start the counter running and enable the input capture channels
  LL_TIM_EnableCounter(TIM2);
  for (int k = 0; k < NUM_HW_CAPTURE; k++) {
    LL_TIM_CC_EnableChannel(TIM2, capture_hw[k].ll_channel);
  }

  // Per in-use TIM2 capture channel: its own circular-DMA channel via
  // the RULOS DMA abstraction (HT/TC callbacks fire at each
  // half-buffer boundary so captures are processed while the other
  // half fills), feeding the owning channel's rising or falling
  // sub-stream.
  static const struct {
    rulos_dma_request_t request;
    volatile uint32_t *ccr;
    uint32_t ll_dma_req_flag;  // LL_TIM_EnableDMAReq_CCn macro arg
  } dma_setup[NUM_HW_CAPTURE] = {
      {RULOS_DMA_REQ_TIM2_CH1, &TIM2->CCR1, TIM_DIER_CC1DE},
      {RULOS_DMA_REQ_TIM2_CH2, &TIM2->CCR2, TIM_DIER_CC2DE},
      {RULOS_DMA_REQ_TIM2_CH3, &TIM2->CCR3, TIM_DIER_CC3DE},
      {RULOS_DMA_REQ_TIM2_CH4, &TIM2->CCR4, TIM_DIER_CC4DE},
  };

  for (int k = 0; k < NUM_HW_CAPTURE; k++) {
    channel_t *ch = &channels[capture_hw[k].channel];
    uint8_t s = capture_hw[k].sub;
    ch->has_hw = true;
    ch->sub[s].buf = capture_buf[k];
    ch->sub[s].polarity_bit =
        (s == SUB_RISING) ? COUNTER_POLARITY_BIT : 0;

    const rulos_dma_config_t cfg = {
        .request = dma_setup[k].request,
        .direction = RULOS_DMA_DIR_PERIPH_TO_MEM,
        .mode = RULOS_DMA_MODE_CIRCULAR,
        .periph_width = RULOS_DMA_WIDTH_WORD,
        .mem_width = RULOS_DMA_WIDTH_WORD,
        .periph_increment = false,
        .mem_increment = true,
        .priority = RULOS_DMA_PRIORITY_VERYHIGH,
        .tc_callback = on_dma,
        .ht_callback = on_dma,
        .user_data = ch,
    };
    ch->sub[s].dma_ch = rulos_dma_alloc(&cfg);
    if (ch->sub[s].dma_ch == NULL) {
      __builtin_trap();
    }
    rulos_dma_start(ch->sub[s].dma_ch, (volatile void *)dma_setup[k].ccr,
                    (void *)capture_buf[k], DMA_CAPTURE_BUFLEN);
    SET_BIT(TIM2->DIER, dma_setup[k].ll_dma_req_flag);
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

// Which sub-streams a slope selects for emission.
static bool sub_active(timestamper_slope_t sl, uint8_t s) {
  if (sl == TIMESTAMPER_SLOPE_BOTH) return true;
  return (sl == TIMESTAMPER_SLOPE_FALLING) ? (s == SUB_FALLING)
                                           : (s == SUB_RISING);
}

void timestamper_set_slope(int ch, timestamper_slope_t slope) {
  if (ch < 0 || ch >= NUM_CHANNELS) return;
  channel_t *c = &channels[ch];
  if (!c->has_hw || c->slope == slope) {
    c->slope = slope;
    return;
  }
  // Both edges are always captured in hardware; slope only selects
  // which sub-streams are emitted. Emit everything captured under the
  // old selection up to now, then skip the newly-selected sub-streams
  // past their pre-command backlog, so the first record emitted under
  // the new slope is an edge captured after the command. (Leaving
  // BOTH may drop a final unpaired edge still held for ordering --
  // one from within the last flush period.)
  __disable_irq();
  service_channel(c, false);
  for (int s = 0; s < NUM_SUBS; s++) {
    if (!sub_active(c->slope, s) && sub_active(slope, s)) {
      c->sub[s].merge_pos = safe_cur(c, s);
      c->sub[s].st_head = c->sub[s].st_n = 0;
    }
  }
  c->slope = slope;
  __enable_irq();
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

// --- Persisted channel config (slope + divider) -------------------
// channels[] is authoritative; flash is its mirror. Slope/divider
// changes only touch RAM -- flash is written solely by the explicit
// CONFig:SAVE command (and *RST, persisting defaults). A flash
// erase/program on this single-bank part stalls instruction fetch for
// tens of ms; doing that on the streaming hot path corrupts capture
// and risks a fault-reset mid-write, so a save instead masks
// interrupts and accepts that pulses during it are lost.

// Persisted payload: per-channel slope + divider. Bump the version
// whenever this layout changes so an older firmware's block is
// rejected (defaults used) rather than misread.
#define TS_CFG_VERSION 1

typedef struct {
  uint8_t slope[NUM_CHANNELS];
  uint32_t divider[NUM_CHANNELS];
} ts_persist_t;

// Mirror of what's currently in flash, so a save can skip a redundant
// erase+program when nothing actually changed.
static ts_persist_t cfg_last_saved;
static bool cfg_last_saved_valid;

static void cfg_snapshot(ts_persist_t *c) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    c->slope[i] = (uint8_t)channels[i].slope;
    c->divider[i] = channels[i].divider;
  }
}

void timestamper_config_save(void) {
  ts_persist_t c;
  cfg_snapshot(&c);
  if (cfg_last_saved_valid &&
      memcmp(&c, &cfg_last_saved, sizeof(c)) == 0) {
    return;  // unchanged -- no flash write
  }
  // Mask interrupts across the flash erase/program: nothing else may
  // run from flash while it's busy. Capture/stream ISRs are silenced
  // for the duration -- pulses arriving now are lost, by design.
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  nvconfig_save(&c, sizeof(c), TS_CFG_VERSION);
  if (!primask) {
    __enable_irq();
  }
  cfg_last_saved = c;
  cfg_last_saved_valid = true;
}

void timestamper_reset_all(void) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    timestamper_set_slope(i, TIMESTAMPER_SLOPE_RISING);
    timestamper_set_divider(i, 1);
  }
  timestamper_set_stream_enabled(true);
  timestamper_set_format(TIMESTAMPER_FORMAT_TEXT);
  timestamper_discard_pending();

  // Full factory reset: persist the defaults now (interrupt-masked,
  // same as CONFig:SAVE).
  timestamper_config_save();
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
    channels[i].count = 0;
    if (!channels[i].has_hw) continue;
    // Skip past everything captured so far and drop any stashed
    // residue: the next edge becomes the first one the host sees.
    for (int s = 0; s < NUM_SUBS; s++) {
      channels[i].sub[s].merge_pos = safe_cur(&channels[i], s);
      channels[i].sub[s].st_head = channels[i].sub[s].st_n = 0;
    }
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

  // Override the compiled defaults with the persisted config if a
  // valid block is in flash. rulos_hal_init() has mapped flash for
  // reads; init_timers() below applies channels[] to the hardware.
  ts_persist_t saved;
  if (nvconfig_load(&saved, sizeof(saved), TS_CFG_VERSION)) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
      channels[i].slope = (timestamper_slope_t)saved.slope[i];
      channels[i].divider = saved.divider[i] ? saved.divider[i] : 1;
    }
  }
  // Seed the saved-mirror with what's now in channels[] (loaded block
  // or compiled defaults) so the first config command doesn't rewrite
  // an identical block.
  cfg_snapshot(&cfg_last_saved);
  cfg_last_saved_valid = true;

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
