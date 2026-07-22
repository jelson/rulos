/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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
 * LectroTIC-4 timestamping firmware.
 *
 * USER-FACING BEHAVIOR
 *
 * The device timestamps input pulses and streams them over USB CDC, relative to firmware start. The
 * timestamp clock is 250 MHz, so each low-order timer tick is 4 ns.
 *
 * The stream has two output formats. Text is human-readable and convenient for logging or terminal
 * use, with diagnostics as "#" comment lines. Binary is compact and faster to drain over USB, with
 * diagnostics as structured in-band records for host tools.
 *
 * Text format is one captured edge per line:
 *
 *   0 5293.585203496 +
 *
 * The fields are channel, decimal seconds with 9 fractional digits, and edge polarity ('+' rising,
 * '-' falling). Binary format uses the same internal 8-byte record that the firmware queues:
 *
 *   uint32 seconds
 *   uint32 counter
 *
 * Normal timestamp counter layout:
 *
 *   [31:30] channel
 *   [29]    edge polarity: 1 = rising '+', 0 = falling '-'
 *   [28]    0
 *   [27:0]  raw 250 MHz tick count within the second
 *
 * Thus, for timestamps, `counter >> 29` is directly `channel * 2 + edge`. If bit 28 is set, the
 * record is a special in-band message instead: counter [7:0] is the message type and `seconds`
 * carries that message's payload.
 *
 * Timing quality depends on a 10 MHz active oscillator (rubidium or GPS-disciplined) on OSC_IN via
 * HSE bypass. The PLL multiplies that reference to 250 MHz SYSCLK. If the external oscillator
 * fails, rulos_hal_init falls back to HSI and sets g_rulos_hse_failed; the periodic task refuses to
 * emit measurement timestamps because HSI accuracy is not meaningful for this instrument.
 *
 * Firmware update over USB DFU (no SWD probe needed; device not capturing):
 *
 *   sudo dfu-util -d 1209:71c4,0483:df11 -a 0 -s 0x08000000:leave -D <fw>.bin
 *
 * 1209:71c4 is the runtime VID:PID, 0483:df11 the ST ROM bootloader. The two-tuple lets dfu-util
 * find the running device, auto-detach it into the bootloader, flash, and restart (:leave). If
 * interrupted it stays in the bootloader and the same command retries. Verify with `tsctl.py idn`.
 *
 * SERIAL INPUT
 *
 * An auxiliary serial input (PA10, 3.3 V logic) timestamps line-oriented data -- log output, NMEA
 * sentences from a GPS receiver -- against the same timebase as the pulse channels. Off by
 * default; SCPI:
 *
 *   SERial:BAUD <rate>      (SERial:BAUD? to query; default 115200)
 *   SERial:STATe ON|OFF     (SERial:STATe? to query; default OFF)
 *
 * Each received line is emitted in-stream, ordered with the pulse timestamps. Text mode:
 *
 *   S 5293.585203496 $GPRMC,...
 *
 * Binary mode: a special record (type 3) whose counter bits [27:8] are the timestamp's tick bits
 * [27:8] (1.024 us resolution) and whose seconds word is the seconds, followed by the line as
 * zero-padded 8-byte groups -- one length byte, then that many line bytes. Timestamps mark line
 * completion as observed at task level: accurate to the scheduler latency (typically well under a
 * millisecond), not capture-grade. Lines arriving faster than USB drains are dropped and counted
 * (type 4 special / "# serial:" comment).
 *
 * The same port's TX pin (PA9) carries the firmware's debug log output; it is a development aid,
 * not part of the public interface. While the serial input is enabled the whole port runs at the
 * configured baud (debug output included); when disabled the port returns to 1 Mbaud.
 *
 * IMPLEMENTATION NOTES
 *
 * The target is an STM32H523, a Cortex-M33 running at 250 MHz. TIM2 is a 32-bit timer configured to
 * roll over once per second, i.e. every 250,000,000 clock ticks. Its input-capture channels latch
 * the counter at the moment an input edge arrives; circular DMA moves those latched CCR values into
 * per-sub-stream buffers.
 *
 * Edge polarity is captured in hardware. TIM2 input capture records only the counter, not the pin
 * state, so each physical input feeds two TIM2 capture channels from the same timer input: one
 * direct-TI channel armed rising and one indirect-TI channel armed falling. Which capture channel
 * fired is the edge polarity. The SCPI slope setting arms only the requested edge directions: a
 * deselected sub-stream's capture channel is disabled outright, so unrequested edges cost no DMA
 * bandwidth and no drain-loop time -- in single-slope mode each pulse costs one capture, not two.
 *
 * Pairing consumes two capture channels per input, so one 4-channel timer serves two inputs. The
 * four jacks split across the two 32-bit timers: TIM2 captures jacks 0 and 2 (PA0/PA2), TIM5
 * captures jacks 1 and 3 (PA1/PA3). TIM5 is phase-locked to TIM2 (TIM2 master, TIM5 slave started
 * by TIM2's TRGO) so all four channels share one timeline and timestamps are directly comparable
 * across channels. See sync_tim5_to_tim2.
 *
 * Emission order is per sub-stream, not global. Each (channel, polarity) sub-stream is emitted in
 * timestamp order, but rising/falling records are not merged into one globally sorted stream. Hosts
 * that need total order sort by timestamp. Doing that on-device would put a record-by-record merge,
 * hold-back rule, stash buffers, quiescence detection, and end-of-burst drains in the capture hot
 * path.
 *
 * The high-order seconds value is maintained in software. A naive rollover interrupt can race with
 * input capture: if TIM2 rolls over after an edge is captured but before firmware pairs that CCR
 * value with the software seconds counter, the timestamp can be off by one second.
 *
 * To avoid that race, the firmware keeps two seconds counters. TIM15 fires twice per TIM2 cycle, at
 * the 1/4 and 3/4 points of each second:
 *
 * - At 3/4 second, TIM2 is in its second half; increment seconds_A so it is ready for the
 *   upcoming rollover.
 * - At 1/4 second, TIM2 is in its first half; copy seconds_A into seconds_B so both counters
 *   agree for the new second.
 *
 * When a captured CCR value is drained, values in the first half of the second use seconds_A and
 * values in the second half use seconds_B. A capture just before rollover that is processed just
 * after rollover still sees a second-half CCR value and therefore uses seconds_B, the pre-rollover
 * seconds value. The 1/4 and 3/4 update points provide a quarter-second ISR latency margin before
 * either value is needed.
 */

#include "timestamper.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "core/dma.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/nvconfig/nvconfig.h"
#include "periph/scpi/scpi.h"
#include "periph/uart/linereader.h"
#include "periph/uart/uart.h"
#include "periph/usb_cdc/usb_cdc.h"
#include "scpi.h"
#include "stm32h5xx_ll_tim.h"

#ifndef RULOS_USE_HSE
#error "timestamper requires RULOS_USE_HSE"
#endif

// NUM_CHANNELS comes from timestamper.h, shared with scpi.c.
#define CLOCK_FREQ_HZ 250000000

// 250 MHz exactly = 4 ns per tick. Used by format_timestamp to convert counter ticks to
// nanoseconds; static_assert below guards against silent breakage if CLOCK_FREQ_HZ changes.
#define NS_PER_TICK 4
_Static_assert(CLOCK_FREQ_HZ == 1000000000 / NS_PER_TICK,
               "format_timestamp's tick->ns conversion assumes 4 ns/tick");

#define TIMESTAMP_PRINT_PERIOD_USEC 100000
// H523 has 256 KB SRAM. The 8 capture buffers (2 sub-streams x 4 channels) and the timestamp ring
// together use ~192 KB, leaving ~64 KB for stack, USB buffers, and globals.
#define TIMESTAMP_BUFLEN   16384  // 16384 * 8 bytes = 128 KB; power of 2 for fast modulo
#define DMA_CAPTURE_BUFLEN 2048   // 8 sub-streams * 2048 * 4 bytes = 64 KB
_Static_assert((DMA_CAPTURE_BUFLEN & (DMA_CAPTURE_BUFLEN - 1)) == 0,
               "cursor arithmetic masks with DMA_CAPTURE_BUFLEN - 1");
// USB FS bulk IN can transfer up to 19 packets (19*64 = 1216 bytes) per 1ms frame. 1280 bytes gives
// headroom to fill a full frame without cutting off mid-timestamp.
#define USB_TX_BUFLEN 1280
#define NUM_TX_BUFS   2

// counter field layout: [31:30] channel, [29] edge polarity, [28] special-message flag, [27:0] tick
// value. The tick value maxes at CLOCK_FREQ_HZ-1 = 249,999,999 (0x0EE6_B27F), which fits in 28
// bits, so extracting it with the 28-bit value mask -- not a 30-bit "everything but channel" mask
// -- keeps the flag bits out of the nanoseconds.
#define COUNTER_CHAN_SHIFT      30
#define COUNTER_CHAN_EDGE_SHIFT 29
#define COUNTER_VALUE_MASK      0x0FFFFFFFU
_Static_assert(CLOCK_FREQ_HZ - 1 <= COUNTER_VALUE_MASK,
               "tick value must fit in the 28-bit counter value field");

// counter [29] = edge polarity of a timestamp record: 1 = rising (LOW->HIGH), 0 = falling.
// Hardware-latched, not inferred: each input feeds two permanently-armed capture channels (one
// rising, one falling), and the bit records which one captured. Special records ([28] set) keep
// this bit zero.
//
// The top three bits are now a direct channel/edge index for normal timestamp records: counter >>
// COUNTER_CHAN_EDGE_SHIFT = (channel * 2) + (rising ? 1 : 0).
#define COUNTER_POLARITY_BIT (1u << 29)

// counter [28] = "special message" marker: the 8-byte record is not a timestamp but device
// metadata. When set, counter [7:0] is the message type and the seconds word carries its payload.
// [31:30] still names the channel the message concerns, [29] remains zero, and channel 0 is used
// when the message is not channel-specific.
#define COUNTER_SPECIAL_BIT  (1u << 28)
#define COUNTER_MSGTYPE_MASK 0xFFu
// all-zero payload, still a full 8-byte record; binary analog of "# output cleared"
#define MSG_OUTPUT_CLEARED 0u
// payload: [15:0] overcaptures, [31:16] buf overflows (each sat. 0xFFFF)
#define MSG_PULSES_LOST 1u
// all-zero payload; reference-clock failure -- the device has halted (binary analog of the "# FATAL
// ..." oscillator line)
#define MSG_OSC_FAIL 2u
// serial line received: counter [27:8] hold the timestamp's tick bits [27:8] (1.024 us
// resolution); the record is followed by ceil((1 + len) / 8) zero-padded 8-byte groups holding one
// length byte and then `len` line bytes
#define MSG_SERIAL_LINE 3u
// payload: number of serial lines dropped since the last report
#define MSG_SERIAL_LOST          4u
#define COUNTER_SERIAL_TICK_MASK 0x0FFFFF00u

// PH1 is freed by HSE bypass (the otherwise-OSC_OUT pin on H5, same trick as the G4 board freeing
// PF1).
#define LED_CHAN0 GPIO_H1  // channel 0
#define LED_CHAN1 GPIO_A4  // channel 1
#define LED_CHAN2 GPIO_A5  // channel 2
#define LED_CHAN3 GPIO_A6  // channel 3
#define LED_USB   GPIO_A7  // USB state

// Two Rev B LED routings must never be driven -- each toggle edge
// phase-slips the PLL by one 10 MHz reference period, corrupting
// timestamps (both measured on hardware):
//  - the clock LED on PC15, which is fed by the current-limited
//    RTC-domain power switch that the datasheet forbids sourcing LED
//    current from;
//  - ch0's activity LED on PH1, the OSC_OUT pin adjacent to the 10 MHz
//    input on PH0 (Rev A shares the PH1 routing but does not exhibit
//    the coupling).
// So on Rev B there is no clock LED and ch0's LED stays dark;
// oscillator failure still shows as the remaining channel LEDs
// flashing in unison. LED_CHAN_FIRST is where the channel-LED loops
// start.
#if defined(TIMESTAMPER_REV_B)
#define LED_CHAN_FIRST 1
#elif defined(TIMESTAMPER_REV_A)
#define LED_CLOCK      GPIO_B5
#define LED_CHAN_FIRST 0
#else
#error "timestamper: board rev not set -- define TIMESTAMPER_REV_A or _REV_B (see SConstruct)"
#endif

// A channel's two hardware capture sub-streams.
#define SUB_RISING  0
#define SUB_FALLING 1
#define NUM_SUBS    2

// In-use capture channels: two per input (rising + falling), four inputs across two 32-bit timers
// -- TIM2 serves jacks 0 and 2, TIM5 serves jacks 1 and 3. 8 sub-streams, 64 KB of capture buffers.
#define NUM_HW_CAPTURE 8
static volatile uint32_t capture_buf[NUM_HW_CAPTURE][DMA_CAPTURE_BUFLEN];

// Per-channel state. Each channel owns two capture sub-streams off the same timer input (one armed
// rising, one armed falling); every record it emits carries a precomputed channel/polarity tag.
// Channels 0 and 2 capture on TIM2 (PA0/PA2), channels 1 and 3 on the phase-locked TIM5 (PA1/PA3).
typedef struct {
  struct {
    volatile uint32_t *buf;  // DMA circular buffer; NULL if no hw
    uint32_t drain_pos;      // next DMA index to drain (circular)
    rulos_dma_channel_t *dma_ch;
    uint32_t counter_tag;  // channel + polarity bits ORed into CCR ticks
  } sub[NUM_SUBS];
  bool has_hw;  // every channel has capture hardware now;
                // kept for the generic init/teardown loops

  // statistics
  uint32_t num_missed;
  uint32_t buf_overflows;

  // Input-capture slope, configurable via SCPI INPut<n>:SLOPe. Only the slope-selected sub-streams
  // are armed in hardware; deselected capture channels are disabled and capture nothing.
  timestamper_slope_t slope;

  // Divider state -- report only every Nth edge, configurable via SCPI INPut<n>:DIVider.
  // divider_set is the SCPI-visible value. In single-slope modes its largest power-of-two factor up
  // to 8 is delegated to the hardware input-capture prescaler (see apply_channel_config) and
  // `divider` holds the software remainder the drain counts; in BOTH mode divider == divider_set.
  // Both 1 means every capture is reported.
  uint32_t divider_set;
  uint32_t divider;
  uint32_t count;

  volatile bool recent_pulse;
} channel_t;

static channel_t channels[NUM_CHANNELS];

// DMA callback context: identifies which channel AND sub-stream a TIM2 capture DMA channel feeds
// (passed as user_data), so the ISR can service the owning channel.
typedef struct {
  channel_t *chan;
  uint8_t sub;
} sub_ctx_t;
static sub_ctx_t sub_ctx[NUM_HW_CAPTURE];

// Continuous timestamp output enable (settable via SCPI OUTPut:STATe). Default ON so a
// freshly-plugged-in device behaves as the user expects.
static bool stream_enabled = true;
static timestamper_format_t format_mode = TIMESTAMPER_FORMAT_TEXT;

// Set by timestamper_discard_pending(); cleared in try_send_timestamps once the marker has been
// written to USB. The marker emission has to bypass stream_enabled because discard_pending always
// silences the stream around the clear.
static volatile bool marker_pending = false;
static const char marker_msg[] = "# output cleared\n";

// Hardware-latched edge polarity: each input drives TWO capture channels off the same timer input
// (TIx) -- one armed rising on the direct TI, one armed falling on the indirect TI (sourced
// internally from the same TIx, so no second pin). Edge direction is implicit in WHICH channel
// captured. Each capture channel keeps its own dedicated DMA request, so raw capture is identical
// to the proven one-sub-per-input path.
//
// Pairing consumes two channels per input, so one 4-channel timer serves two inputs. The four
// front-panel jacks split across the two 32-bit timers, phase-locked (see sync_tim5_to_tim2): TIM2
// takes the odd timer-inputs TI1/TI3 (PA0 = jack 0, PA2 = jack 2) on AF1, TIM5 the even TI2/TI4
// (PA1 = jack 1, PA3 = jack 3) on AF2. The direct-TI channel of an input carries its rising
// sub-stream and needs the pin; the indirect channel (gpio_pin == 0) carries falling off the same
// TI.
//   TIM2 CH1 direct  / CH2 indirect  -> jack 0 (TI1 = PA0)
//   TIM2 CH3 direct  / CH4 indirect  -> jack 2 (TI3 = PA2)
//   TIM5 CH2 direct  / CH1 indirect  -> jack 1 (TI2 = PA1)
//   TIM5 CH4 direct  / CH3 indirect  -> jack 3 (TI4 = PA3)
static const struct {
  TIM_TypeDef *tim;        // owning timer (TIM2 or TIM5)
  uint32_t af;             // LL_GPIO_AF_1 (TIM2) / LL_GPIO_AF_2 (TIM5)
  uint32_t gpio_pin;       // LL_GPIO_PIN_n, or 0 for indirect (no pin)
  uint32_t ll_channel;     // LL_TIM_CHANNEL_CHn
  uint32_t active_input;   // LL_TIM_ACTIVEINPUT_DIRECTTI / _INDIRECTTI
  uint32_t ic_polarity;    // LL_TIM_IC_POLARITY_RISING / _FALLING
  volatile uint32_t *ccr;  // &TIMx->CCRn -- the DMA source register
  uint32_t dier_bit;       // TIM_DIER_CCnDE
  uint32_t sr_of_bit;      // TIM_SR_CCnOF (overcapture flag)
  rulos_dma_request_t request;
  uint8_t channel;  // owning channel
  uint8_t sub;      // SUB_RISING / SUB_FALLING
} capture_hw[NUM_HW_CAPTURE] = {
    // --- TIM2 (AF1): jack 0 on TI1 = PA0, jack 2 on TI3 = PA2 ---
    {.tim = TIM2,
     .af = LL_GPIO_AF_1,
     .gpio_pin = LL_GPIO_PIN_0,
     .ll_channel = LL_TIM_CHANNEL_CH1,
     .active_input = LL_TIM_ACTIVEINPUT_DIRECTTI,
     .ic_polarity = LL_TIM_IC_POLARITY_RISING,
     .ccr = &TIM2->CCR1,
     .dier_bit = TIM_DIER_CC1DE,
     .sr_of_bit = TIM_SR_CC1OF,
     .request = RULOS_DMA_REQ_TIM2_CH1,
     .channel = 0,
     .sub = SUB_RISING},
    {.tim = TIM2,
     .af = LL_GPIO_AF_1,
     .gpio_pin = 0,
     .ll_channel = LL_TIM_CHANNEL_CH2,
     .active_input = LL_TIM_ACTIVEINPUT_INDIRECTTI,
     .ic_polarity = LL_TIM_IC_POLARITY_FALLING,
     .ccr = &TIM2->CCR2,
     .dier_bit = TIM_DIER_CC2DE,
     .sr_of_bit = TIM_SR_CC2OF,
     .request = RULOS_DMA_REQ_TIM2_CH2,
     .channel = 0,
     .sub = SUB_FALLING},
    {.tim = TIM2,
     .af = LL_GPIO_AF_1,
     .gpio_pin = LL_GPIO_PIN_2,
     .ll_channel = LL_TIM_CHANNEL_CH3,
     .active_input = LL_TIM_ACTIVEINPUT_DIRECTTI,
     .ic_polarity = LL_TIM_IC_POLARITY_RISING,
     .ccr = &TIM2->CCR3,
     .dier_bit = TIM_DIER_CC3DE,
     .sr_of_bit = TIM_SR_CC3OF,
     .request = RULOS_DMA_REQ_TIM2_CH3,
     .channel = 2,
     .sub = SUB_RISING},
    {.tim = TIM2,
     .af = LL_GPIO_AF_1,
     .gpio_pin = 0,
     .ll_channel = LL_TIM_CHANNEL_CH4,
     .active_input = LL_TIM_ACTIVEINPUT_INDIRECTTI,
     .ic_polarity = LL_TIM_IC_POLARITY_FALLING,
     .ccr = &TIM2->CCR4,
     .dier_bit = TIM_DIER_CC4DE,
     .sr_of_bit = TIM_SR_CC4OF,
     .request = RULOS_DMA_REQ_TIM2_CH4,
     .channel = 2,
     .sub = SUB_FALLING},
    // --- TIM5 (AF2): jack 1 on TI2 = PA1, jack 3 on TI4 = PA3 ---
    {.tim = TIM5,
     .af = LL_GPIO_AF_2,
     .gpio_pin = LL_GPIO_PIN_1,
     .ll_channel = LL_TIM_CHANNEL_CH2,
     .active_input = LL_TIM_ACTIVEINPUT_DIRECTTI,
     .ic_polarity = LL_TIM_IC_POLARITY_RISING,
     .ccr = &TIM5->CCR2,
     .dier_bit = TIM_DIER_CC2DE,
     .sr_of_bit = TIM_SR_CC2OF,
     .request = RULOS_DMA_REQ_TIM5_CH2,
     .channel = 1,
     .sub = SUB_RISING},
    {.tim = TIM5,
     .af = LL_GPIO_AF_2,
     .gpio_pin = 0,
     .ll_channel = LL_TIM_CHANNEL_CH1,
     .active_input = LL_TIM_ACTIVEINPUT_INDIRECTTI,
     .ic_polarity = LL_TIM_IC_POLARITY_FALLING,
     .ccr = &TIM5->CCR1,
     .dier_bit = TIM_DIER_CC1DE,
     .sr_of_bit = TIM_SR_CC1OF,
     .request = RULOS_DMA_REQ_TIM5_CH1,
     .channel = 1,
     .sub = SUB_FALLING},
    {.tim = TIM5,
     .af = LL_GPIO_AF_2,
     .gpio_pin = LL_GPIO_PIN_3,
     .ll_channel = LL_TIM_CHANNEL_CH4,
     .active_input = LL_TIM_ACTIVEINPUT_DIRECTTI,
     .ic_polarity = LL_TIM_IC_POLARITY_RISING,
     .ccr = &TIM5->CCR4,
     .dier_bit = TIM_DIER_CC4DE,
     .sr_of_bit = TIM_SR_CC4OF,
     .request = RULOS_DMA_REQ_TIM5_CH4,
     .channel = 3,
     .sub = SUB_RISING},
    {.tim = TIM5,
     .af = LL_GPIO_AF_2,
     .gpio_pin = 0,
     .ll_channel = LL_TIM_CHANNEL_CH3,
     .active_input = LL_TIM_ACTIVEINPUT_INDIRECTTI,
     .ic_polarity = LL_TIM_IC_POLARITY_FALLING,
     .ccr = &TIM5->CCR3,
     .dier_bit = TIM_DIER_CC3DE,
     .sr_of_bit = TIM_SR_CC3OF,
     .request = RULOS_DMA_REQ_TIM5_CH3,
     .channel = 3,
     .sub = SUB_FALLING},
};

// total seconds elapsed since boot -- for details, see comment at top. volatile because TIM15 ISR
// writes these and resolve() reads them from both DMA ISR and main-thread (flush) context.
volatile uint32_t seconds_A = 0;
volatile uint32_t seconds_B = 0;

// Circular buffer for timestamps not yet written to USB
typedef struct {
  uint32_t seconds;
  uint32_t counter;  // [31:30] channel, [29] edge polarity,
                     // [28] special-message flag, [27:0] tick value / message type (see the
                     // COUNTER_* / MSG_* defines above)
} timestamp_t;

timestamp_t timestamp_buffer[TIMESTAMP_BUFLEN];
static volatile uint32_t ts_head = 0;  // next write position (ISR)
static volatile uint32_t ts_tail = 0;  // next read position (USB output)

// Output devices. The USB CDC connection is owned by the SCPI library (src/lib/periph/scpi); this
// app shares the same connection for streaming timestamps using scpi_usb_cdc_handle().
static UartState_t uart;

// ---- Serial input (see SERIAL INPUT in the header comment) ---------------------------------
#define SERIAL_DEBUG_BAUD   1000000  // debug-log baud while the serial input is off
#define SERIAL_DEFAULT_BAUD 115200
#define SERIAL_LINE_SLOTS   8

typedef struct {
  uint8_t len;
  char text[LINEREADER_MAX_LINE_LEN];
} serial_line_t;
_Static_assert(LINEREADER_MAX_LINE_LEN <= 256,
               "serial line length must fit the wire format's one-byte length field");

// Line texts ride this queue while their MSG_SERIAL_LINE entries travel the timestamp ring; both
// are FIFO and produced together, so fill_tx_buf pops them in lockstep. Every access is task
// context (linereader upcall, fill, discard) -- only the ring push needs the irq guard.
static serial_line_t serial_lines[SERIAL_LINE_SLOTS];
static uint32_t serial_line_head = 0, serial_line_tail = 0;
static uint32_t serial_drops = 0;
static bool serial_enabled = false;
static uint32_t serial_baud = SERIAL_DEFAULT_BAUD;
static LineReader_t serial_linereader;

// Ping-pong USB TX buffers. While USB is sending one buffer, the periodic task fills the other with
// formatted timestamps. When the TX complete callback fires, the pre-filled buffer is handed to USB
// immediately — no formatting delay, so we never miss a USB frame.
static char usb_tx_bufs[NUM_TX_BUFS][USB_TX_BUFLEN];
static int tx_filling = 0;   // index of buffer currently being filled
static int tx_fill_pos = 0;  // bytes written into the filling buffer

// TIM15 fires twice per rollover of TIM2, the input capture timer. It is used to update the high
// order bits. For details, see comment at top.
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

// DMA-safe end-of-region for sub `s`: records [drain_pos, cur) are captured and safe to read (cur
// is exclusive). Single-owner cursor: per-channel service is serialized by the same-priority DMA
// ISRs plus the irq-guarded flush.
CCMRAM static inline uint32_t safe_cur(channel_t *chan, uint8_t s) {
  uint32_t rem = rulos_dma_get_remaining(chan->sub[s].dma_ch);
  uint32_t cur = DMA_CAPTURE_BUFLEN - rem;
  return (cur == DMA_CAPTURE_BUFLEN) ? 0 : cur;
}

// Drain sub `s` from drain_pos up to cur straight into the timestamp ring -- the single-slope hot
// path. Capture sustains one record per 100 ns across a 16k-record burst, so the inner loop must
// stay at the ~16-cycle cost of a load, an A/B seconds select, two stores, and a masked increment:
// ring space is pre-checked per batch, the precomputed channel/polarity tag is loaded once, and
// recent_pulse is set once. Records that don't fit in the ring are dropped and counted.
CCMRAM static void drain_sub_fast(channel_t *chan, uint8_t s, uint32_t cur) {
  uint32_t pos = chan->sub[s].drain_pos;
  if (pos == cur) {
    return;
  }
  chan->recent_pulse = true;
  chan->sub[s].drain_pos = cur;

  uint32_t count = (cur - pos) & (DMA_CAPTURE_BUFLEN - 1);
  uint32_t head = ts_head;
  uint32_t available = (ts_tail - head - 1) % TIMESTAMP_BUFLEN;
  if (__builtin_expect(count > available, 0)) {
    chan->buf_overflows += count - available;
    count = available;
    if (count == 0) {
      return;
    }
  }

  const uint32_t tag = chan->sub[s].counter_tag;
  const uint32_t divider = chan->divider;
  volatile uint32_t *const buf = chan->sub[s].buf;

  while (count > 0) {
    uint32_t seg = DMA_CAPTURE_BUFLEN - pos;
    if (seg > count) {
      seg = count;
    }
    // Pointer-based iteration so the compiler doesn't spill the count.
    volatile uint32_t *src = buf + pos;
    volatile uint32_t *const end = src + seg;
    while (src < end) {
      uint32_t counter = *src++;
      uint32_t seconds = counter < (CLOCK_FREQ_HZ / 2) ? seconds_A : seconds_B;
      if (__builtin_expect(divider != 1, 0)) {
        chan->count++;
        if (chan->count < divider) {
          continue;
        }
        chan->count = 0;
      }
      timestamp_buffer[head].seconds = seconds;
      timestamp_buffer[head].counter = counter | tag;
      head = (head + 1) % TIMESTAMP_BUFLEN;
    }
    pos = (pos + seg) & (DMA_CAPTURE_BUFLEN - 1);
    count -= seg;
  }
  ts_head = head;
}

// Which sub-streams a slope selects. A selected sub-stream is armed in hardware and emitted; a
// deselected one's capture channel is disabled.
static bool sub_active(timestamper_slope_t sl, uint8_t s) {
  if (sl == TIMESTAMPER_SLOPE_BOTH) {
    return true;
  }
  return (sl == TIMESTAMPER_SLOPE_FALLING) ? (s == SUB_FALLING) : (s == SUB_RISING);
}

// Arm or disarm capture sub-stream k. A disabled capture channel latches nothing: no CCR update, no
// DMA request, no overcapture flag. Its GPDMA channel stays armed but starves, so the drain cursor
// stays in sync across disable/enable. On re-arm, clear the sub's stale capture/overcapture flags
// (CCnIF is CCnOF >> 8 in TIM_SR) so a value latched before the last disable isn't misread as a
// fresh edge.
static void set_capture_enabled(int k, bool on) {
  if (on) {
    capture_hw[k].tim->SR = ~(capture_hw[k].sr_of_bit | (capture_hw[k].sr_of_bit >> 8));
    LL_TIM_CC_EnableChannel(capture_hw[k].tim, capture_hw[k].ll_channel);
  } else {
    LL_TIM_CC_DisableChannel(capture_hw[k].tim, capture_hw[k].ll_channel);
  }
}

// Service a channel: drain each slope-selected sub-stream straight into the ring. A deselected
// sub-stream is disarmed and captures nothing, but its cursor is still resynced here: a capture in
// flight at the instant of disarm can land one final buffer entry, which this skips so it can't be
// misread later. Each sub-stream is emitted in time order; the two sub-streams are NOT interleaved
// into one ordered sequence (see the top-of-file EMISSION ORDER note).
//
// The drain assumes the input stays within the capture envelope. The cursor arithmetic is modular:
// if a sub's write head ever gained a full buffer on its cursor between services, the overrun would
// alias to a small advance and the overwritten records would go unnoticed. In-regime inputs can't
// get there -- every half-buffer crossing wakes this drain, which empties faster than a spec'd
// input can fill.
CCMRAM static void service_channel(channel_t *chan) {
  for (uint8_t s = 0; s < NUM_SUBS; s++) {
    uint32_t cur = safe_cur(chan, s);
    if (sub_active(chan->slope, s)) {
      drain_sub_fast(chan, s, cur);
    } else {
      chan->sub[s].drain_pos = cur;
    }
  }
}

// TIM2 capture DMA HT/TC callback (one per in-use sub). The NDTR- driven drain doesn't distinguish
// HT from TC, so both map here as pure wake-ups. All capture DMA ISRs share one NVIC priority
// (cannot preempt each other), so per-channel service is serialized without masking.
CCMRAM static void on_dma(void *user_data) {
  sub_ctx_t *ctx = (sub_ctx_t *)user_data;
  service_channel(ctx->chan);
}

static uint32_t ic_prescaler_ll(uint32_t hw) {
  switch (hw) {
    case 2:
      return LL_TIM_ICPSC_DIV2;
    case 4:
      return LL_TIM_ICPSC_DIV4;
    case 8:
      return LL_TIM_ICPSC_DIV8;
    default:
      return LL_TIM_ICPSC_DIV1;
  }
}

// Apply a channel's slope and divider to hardware. The divider is split into a hardware
// input-capture prescaler (ICxPSC: capture every 1st/2nd/4th/8th edge) and a software remainder
// counted by the drain. A prescaled-away edge costs no CCR latch, no DMA transfer and no drain
// time, so the hardware share raises the channel's continuous divided-input ceiling by up to 8x.
// Only single-slope modes take the hardware share: the BOTH-mode divider counts interleaved edges
// of both polarities with one counter, which per-sub-stream prescalers can't express.
//
// Sequence: disarm the channel's capture pair, emit the backlog captured under the old
// configuration, reprogram, then re-sync both cursors and arm the subs the new slope selects -- so
// the first record emitted under the new configuration is an edge captured after the command.
// Disarming a capture channel (CCxE = 0) also resets its prescaler's internal edge count, so the
// hardware share starts a fresh group just like the zeroed software count.
static void apply_channel_config(int ch, timestamper_slope_t slope) {
  channel_t *c = &channels[ch];
  uint32_t hw = 1;
  if (slope != TIMESTAMPER_SLOPE_BOTH) {
    if (c->divider_set % 8 == 0) {
      hw = 8;
    } else if (c->divider_set % 4 == 0) {
      hw = 4;
    } else if (c->divider_set % 2 == 0) {
      hw = 2;
    }
  }
  __disable_irq();
  for (int k = 0; k < NUM_HW_CAPTURE; k++) {
    if (capture_hw[k].channel == ch) {
      set_capture_enabled(k, false);
    }
  }
  service_channel(c);
  c->slope = slope;
  c->divider = c->divider_set / hw;
  c->count = 0;
  for (int k = 0; k < NUM_HW_CAPTURE; k++) {
    if (capture_hw[k].channel != ch) {
      continue;
    }
    uint8_t s = capture_hw[k].sub;
    LL_TIM_IC_SetPrescaler(capture_hw[k].tim, capture_hw[k].ll_channel, ic_prescaler_ll(hw));
    c->sub[s].drain_pos = safe_cur(c, s);
    if (sub_active(slope, s)) {
      set_capture_enabled(k, true);
    }
  }
  __enable_irq();
}

// Format a timestamp into buf, returning the number of bytes written. Branches on format_mode set
// by SCPI.
//
// TEXT: at 250 MHz, each tick is 4 ns. Counter ranges 0..249,999,999; counter * 4 yields
// nanoseconds in 0..999,999,996, within uint32_t. 9 fractional digits give nanosecond resolution.
// The trailing column is the hardware-latched edge polarity: '+' rising, '-' falling.
//
// BINARY: 8 bytes -- the on-device timestamp_t layout (4 bytes seconds LE, then 4 bytes counter LE
// with channel and polarity in the top 3 bits) is the wire format. memcpy straight out, skipping
// the 9-digit divmod chain in snprintf entirely.
static int format_timestamp(timestamp_t *t, char *buf, int buf_size) {
  if (format_mode == TIMESTAMPER_FORMAT_BINARY) {
    _Static_assert(sizeof(timestamp_t) == TIMESTAMPER_BINARY_RECORD_LEN,
                   "binary wire format mirrors timestamp_t layout");
    if (buf_size < (int)sizeof(timestamp_t)) {
      return 0;
    }
    memcpy(buf, t, sizeof(timestamp_t));
    return sizeof(timestamp_t);
  }
  uint8_t channel_edge = t->counter >> COUNTER_CHAN_EDGE_SHIFT;
  uint8_t channel = channel_edge >> 1;
  char polarity = (channel_edge & 1) ? '+' : '-';
  uint32_t counter = t->counter & COUNTER_VALUE_MASK;
  uint32_t nanoseconds = counter * NS_PER_TICK;
  return snprintf(buf, buf_size, "%d %lu.%09lu %c\n", channel, t->seconds, nanoseconds, polarity);
}

static bool received_recent_pulse(uint8_t channel_num) {
  if (channels[channel_num].recent_pulse) {
    channels[channel_num].recent_pulse = false;
    return true;
  }

  return false;
}

// USB activity for the LED is tracked inside the SCPI library; we just drain its sticky bit each
// LED update via scpi_consume_recent_activity.

// Populated at startup by main(). Can't be a static const initializer because GPIO_An are
// themselves `static const gpio_pin_t` and so not constant expressions in the C sense.
static gpio_pin_t led_chan_pins[NUM_CHANNELS];

static void update_leds(void) {
  static bool on_phase = false;
  on_phase = !on_phase;

#ifdef LED_CLOCK
  // 10 MHz HSE health: a steady heartbeat regardless of input activity. Off when HSE has failed.
  if (g_rulos_hse_failed) {
    gpio_clr(LED_CLOCK);
  } else {
    gpio_set_or_clr(LED_CLOCK, on_phase);
  }
#endif

  // Each activity LED has an idle state and a "blink" state opposite to idle. The blink shows up
  // during whichever phase is opposite the idle level: on_phase for channels (idle off), off_phase
  // for USB (idle on). During the other phase the LED rests at idle.
  //
  // Continuous activity sets the LED's "blink" state every cycle, so both lights flash 5 Hz in
  // phase with the clock LED. A single pulse produces one visible 100 ms blink the next time the
  // LED's active phase comes around.

  for (int i = LED_CHAN_FIRST; i < NUM_CHANNELS; i++) {
    gpio_set_or_clr(led_chan_pins[i], on_phase && received_recent_pulse(i));
  }

  const bool usb_blink_off = !on_phase && scpi_consume_recent_activity();
  gpio_set_or_clr(LED_USB, scpi_usb_ready() && !usb_blink_off);
}

static void serial_line_cb(UartState_t *u, void *user_data, char *line) {
  if (!serial_enabled) {
    return;
  }
  uint32_t next_slot = (serial_line_head + 1) % SERIAL_LINE_SLOTS;
  if (next_slot == serial_line_tail) {
    serial_drops++;
    return;
  }
  serial_line_t *slot = &serial_lines[serial_line_head];
  size_t len = 0;
  while (line[len] != 0 && len < sizeof(slot->text) - 1) {
    slot->text[len] = line[len];
    len++;
  }
  slot->len = len;

  // The ring push races the capture ISRs; the timestamp is read in the same guarded section so it
  // can't straddle a TIM15 seconds update.
  __disable_irq();
  uint32_t ticks = TIM2->CNT;
  uint32_t secs = ticks < (CLOCK_FREQ_HZ / 2) ? seconds_A : seconds_B;
  uint32_t next_ring = (ts_head + 1) % TIMESTAMP_BUFLEN;
  if (next_ring == ts_tail) {
    __enable_irq();
    serial_drops++;
    return;
  }
  timestamp_buffer[ts_head].seconds = secs;
  timestamp_buffer[ts_head].counter =
      COUNTER_SPECIAL_BIT | (ticks & COUNTER_SERIAL_TICK_MASK) | MSG_SERIAL_LINE;
  ts_head = next_ring;
  __enable_irq();
  serial_line_head = next_slot;
}

void timestamper_serial_set_baud(uint32_t baud) {
  serial_baud = baud;
  if (serial_enabled) {
    uart_set_baud(&uart, baud);
  }
}

uint32_t timestamper_serial_get_baud(void) {
  return serial_baud;
}

void timestamper_serial_set_enabled(bool enabled) {
  if (enabled == serial_enabled) {
    return;
  }
  if (enabled) {
    // RX starts once and stays armed; the enable flag gates line handling.
    static bool rx_started = false;
    if (!rx_started) {
      rx_started = true;
      linereader_init(&serial_linereader, &uart, serial_line_cb, NULL);
    }
    uart_set_baud(&uart, serial_baud);
    serial_enabled = true;
  } else {
    serial_enabled = false;
    uart_set_baud(&uart, SERIAL_DEBUG_BAUD);
  }
}

bool timestamper_serial_get_enabled(void) {
  return serial_enabled;
}

// Format ring records into the current filling buffer until it's full or the ring is empty. Called
// eagerly from periodic_task so the buffer is pre-filled by the time USB finishes the previous
// send. A record is peeked first and popped only once it is known to fit, so a serial line and its
// framing never split across buffers.
static void fill_tx_buf(void) {
  while (ts_tail != ts_head) {
    timestamp_t t = timestamp_buffer[ts_tail];
    const bool is_serial =
        (t.counter & COUNTER_SPECIAL_BIT) && (t.counter & COUNTER_MSGTYPE_MASK) == MSG_SERIAL_LINE;
    // Always a valid pointer (only read when is_serial): the compiler may hoist the len load
    // above the is_serial select, and a speculated NULL load hard-faults on the M33.
    const serial_line_t *line = &serial_lines[serial_line_tail];
    int need;
    if (format_mode == TIMESTAMPER_FORMAT_BINARY) {
      need = sizeof(timestamp_t) + (is_serial ? 8 * ((1 + line->len + 7) / 8) : 0);
    } else {
      // Timestamp: "3 4294967295.999999996 +\n" = 25 bytes. Serial: "S ", the same seconds field,
      // the line, "\n", NUL.
      need = is_serial ? 25 + line->len : 25;
    }
    if (tx_fill_pos + need > USB_TX_BUFLEN) {
      break;
    }
    ts_tail = (ts_tail + 1) % TIMESTAMP_BUFLEN;
    char *out = usb_tx_bufs[tx_filling] + tx_fill_pos;
    if (!is_serial) {
      tx_fill_pos += format_timestamp(&t, out, USB_TX_BUFLEN - tx_fill_pos);
      continue;
    }
    serial_line_tail = (serial_line_tail + 1) % SERIAL_LINE_SLOTS;
    if (format_mode == TIMESTAMPER_FORMAT_BINARY) {
      memcpy(out, &t, sizeof(timestamp_t));
      out += sizeof(timestamp_t);
      int groups = (1 + line->len + 7) / 8;
      memset(out, 0, 8 * groups);
      out[0] = line->len;
      memcpy(out + 1, line->text, line->len);
      tx_fill_pos += sizeof(timestamp_t) + 8 * groups;
    } else {
      uint32_t ns = (t.counter & COUNTER_SERIAL_TICK_MASK) * NS_PER_TICK;
      tx_fill_pos +=
          snprintf(out, USB_TX_BUFLEN - tx_fill_pos, "S %lu.%09lu %.*s\n", (unsigned long)t.seconds,
                   (unsigned long)ns, (int)line->len, line->text);
    }
  }
}

// Emit one 8-byte special-message record on the binary stream. The buffer is static because
// usbd_cdc_write is asynchronous (DMA): it must outlive the transfer. Safe as a single instance
// because callers gate on USB idle and send at most one per service period, exactly like the text
// marker / diagnostic line.
static void send_special(uint8_t channel, uint8_t msgtype, uint32_t payload) {
  static timestamp_t msg;
  msg.seconds = payload;
  msg.counter = ((uint32_t)channel << COUNTER_CHAN_SHIFT) | COUNTER_SPECIAL_BIT | msgtype;
  usbd_cdc_write(scpi_usb_cdc_handle(), (uint8_t *)&msg, sizeof(msg));
}

// If the filling buffer has data and USB is idle, hand it off and swap to the other buffer. Then
// eagerly start filling the new buffer so it's ready when USB finishes.
static void try_send_timestamps(void) {
  // The marker bypasses stream_enabled so the host can synchronize even after silencing the stream
  // during a discard_pending() dance. Binary mode gets it as a special record, text mode as the
  // comment.
  if (marker_pending && usbd_cdc_tx_ready(scpi_usb_cdc_handle())) {
    marker_pending = false;
    if (format_mode == TIMESTAMPER_FORMAT_BINARY) {
      send_special(0, MSG_OUTPUT_CLEARED, 0);
    } else {
      usbd_cdc_write(scpi_usb_cdc_handle(), (uint8_t *)marker_msg, sizeof(marker_msg) - 1);
    }
    return;
  }
  if (!stream_enabled) {
    return;
  }
  if (tx_fill_pos == 0 || !usbd_cdc_tx_ready(scpi_usb_cdc_handle())) {
    return;
  }

  usbd_cdc_write(scpi_usb_cdc_handle(), usb_tx_bufs[tx_filling], tx_fill_pos);

  // Swap: the buffer we just handed to USB is now "sending" (owned by the USB stack). Start filling
  // the other one.
  tx_filling = 1 - tx_filling;
  tx_fill_pos = 0;
  fill_tx_buf();
}

// Forwarded by the SCPI library after each USB CDC TX completes. Drives the ping-pong handoff: as
// soon as the in-flight buffer drains, we pre-arm the freshly-filled one.
static void on_usb_tx_complete(void) {
  try_send_timestamps();
}

// Periodic catch-up: drain each channel promptly even when the DMA buffers fill slowly (the HT/TC
// ISR alone could otherwise leave a slow signal's captures waiting up to a half-buffer). Called
// from periodic_task.
static void flush_dma_captures(void) {
  // Capture overrun: a CCxOF flag means an edge arrived before GPDMA fetched the previous capture
  // from that channel's CCR -- the older edge's timestamp is gone. The flag is sampled, not a
  // counter, so this undercounts at extreme rates; the wire report saturates anyway, communicating
  // "many". Each capture sub-stream owns one CCxOF bit on its timer (TIM2 or TIM5).
  for (int k = 0; k < NUM_HW_CAPTURE; k++) {
    if (__builtin_expect(capture_hw[k].tim->SR & capture_hw[k].sr_of_bit, 0)) {
      capture_hw[k].tim->SR = ~capture_hw[k].sr_of_bit;  // rc_w0
      channels[capture_hw[k].channel].num_missed++;
    }
  }

  for (int i = 0; i < NUM_CHANNELS; i++) {
    channel_t *ch = &channels[i];
    if (!ch->has_hw) {
      continue;
    }

    // Disable interrupts so we don't race the DMA ISR's call into service_channel (shared drain_pos
    // / ring head).
    __disable_irq();
    service_channel(ch);
    __enable_irq();
  }
}

static void periodic_task(void *data) {
  schedule_us(TIMESTAMP_PRINT_PERIOD_USEC, periodic_task, NULL);

  // HSE failed at boot or mid-stream (the latter detected by CSS via the NMI handler in
  // stm32-hardware.c). HSI fallback can't produce measurement-accurate timestamps, so refuse to
  // operate and surface the error to the user. The clock LED (if this rev has one) goes off in
  // update_leds, and we flash the channel LEDs in unison as a "this device is broken" indicator.
  if (g_rulos_hse_failed) {
    LL_TIM_DisableCounter(TIM2);
    LL_TIM_DisableCounter(TIM5);
    LL_TIM_DisableCounter(TIM15);
    for (int i = 0; i < NUM_CHANNELS; i++) {
      if (!channels[i].has_hw) {
        continue;
      }
      for (int s = 0; s < NUM_SUBS; s++) {
        rulos_dma_stop(channels[i].sub[s].dma_ch);
      }
    }
    LL_TIM_DisableIT_UPDATE(TIM15);

    static bool on = false;
    on = !on;
    for (int i = LED_CHAN_FIRST; i < NUM_CHANNELS; i++) {
      gpio_set_or_clr(led_chan_pins[i], on);
    }

    static bool error_printed = false;
    if (!error_printed && usbd_cdc_tx_ready(scpi_usb_cdc_handle())) {
      error_printed = true;
      if (format_mode == TIMESTAMPER_FORMAT_BINARY) {
        send_special(0, MSG_OSC_FAIL, 0);
      } else {
        usbd_cdc_print(
            scpi_usb_cdc_handle(),
            "# FATAL: External oscillator failure. Connect a 10MHz source and press reset.\n");
      }
    }
    return;
  }

  // If USB has just come up for the first time, print a welcome message
  static bool welcome_printed = false;
  if (!welcome_printed && format_mode == TIMESTAMPER_FORMAT_TEXT &&
      usbd_cdc_tx_ready(scpi_usb_cdc_handle())) {
    usbd_cdc_print(scpi_usb_cdc_handle(), "# Starting LectroTIC-4, version " TIMESTAMPER_FW_VERSION
                                          "-" STRINGIFY(GIT_COMMIT) "\n");
    welcome_printed = true;
  }

  flush_dma_captures();
  update_leds();

  // Report missed pulses when USB is idle and no timestamps pending: a "# ch<N>: ..." line in text
  // mode, a PULSES_LOST special record in binary mode. Suppressed while streaming is disabled so
  // SCPI query responses aren't interleaved with overflow noise.
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
          // The filling buffer is free here -- nothing else writes it in this path.
          int len = snprintf(usb_tx_bufs[tx_filling], USB_TX_BUFLEN,
                             "# ch%d: %lu overcaptures, %lu buf overflows\n", i, missed, overflows);
          usbd_cdc_write(scpi_usb_cdc_handle(), usb_tx_bufs[tx_filling], len);
          tx_filling = 1 - tx_filling;
        }
        return;  // one message per period; USB will drain the rest
      }
    }
    if (serial_drops) {
      uint32_t dropped = serial_drops;
      serial_drops = 0;
      if (format_mode == TIMESTAMPER_FORMAT_BINARY) {
        send_special(0, MSG_SERIAL_LOST, dropped);
      } else {
        int len = snprintf(usb_tx_bufs[tx_filling], USB_TX_BUFLEN, "# serial: %lu lines dropped\n",
                           (unsigned long)dropped);
        usbd_cdc_write(scpi_usb_cdc_handle(), usb_tx_bufs[tx_filling], len);
        tx_filling = 1 - tx_filling;
      }
      return;
    }
  }

  fill_tx_buf();
  try_send_timestamps();
}

// Configure a capture timer's time base: 32-bit up-counter, no prescaler, rolling over once per
// second (ARR = clock - 1). Leaves the counter disabled so the caller can start TIM2 and TIM5 in a
// phase-locked pair.
static void init_capture_timebase(TIM_TypeDef *tim) {
  LL_TIM_InitTypeDef tb = {
      .Prescaler = 0,
      .CounterMode = LL_TIM_COUNTERMODE_UP,
      .Autoreload = CLOCK_FREQ_HZ - 1,
      .ClockDivision = LL_TIM_CLOCKDIVISION_DIV1,
      .RepetitionCounter = 0,
  };
  LL_TIM_Init(tim, &tb);
}

// Phase-lock TIM5 to TIM2 so both 32-bit counters share one timeline: the four channels stay
// coherent (a CH0 capture and a CH1 capture are directly subtractable) regardless of which timer
// captured.
//
// TIM2 is the master: its TRGO is driven by the ENABLE event, so the instant TIM2's counter is
// enabled it emits a trigger. TIM5 is a slave in trigger mode off that signal -- per RM0481 Table
// 421 (TIM2/3/4/5 internal trigger connection), TIM2's trgo reaches TIM5 on tim_itr1 -- so TIM5's
// counter starts on the same clock edge TIM2 starts. Both run from the same APB timer clock at the
// same ARR, so once started together they stay bit-for-bit aligned with no drift. The only offset
// is the fixed trigger-propagation latency (about one timer tick), which is a constant
// cross-timer-group bias a host can calibrate out; within a timer group (CH0 vs CH2, or CH1 vs CH3)
// the offset is exactly zero.
static void sync_tim5_to_tim2(void) {
  LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_ENABLE);
  LL_TIM_DisableMasterSlaveMode(TIM2);
  LL_TIM_SetTriggerInput(TIM5, LL_TIM_TS_ITR1);
  LL_TIM_SetSlaveMode(TIM5, LL_TIM_SLAVEMODE_TRIGGER);
}

static void init_timers() {
  __HAL_RCC_TIM2_CLK_ENABLE();
  __HAL_RCC_TIM5_CLK_ENABLE();
  init_capture_timebase(TIM2);
  init_capture_timebase(TIM5);
  sync_tim5_to_tim2();

  // Each input's rising sub-stream uses a direct-TI pin configured for AF input (TIM2 = AF1, TIM5 =
  // AF2); its falling sub-stream is sourced from the same TI internally (indirect, gpio_pin == 0)
  // and needs no pin. The pin is sensed, not driven. Each capture channel's polarity is fixed in
  // hardware -- the SCPI slope picks which of the pair are armed (see set_capture_enabled), never
  // how a channel is configured.
  LL_GPIO_InitTypeDef gpio_init = {0};
  gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
  gpio_init.Pull = LL_GPIO_PULL_DOWN;

  for (int k = 0; k < NUM_HW_CAPTURE; k++) {
    if (capture_hw[k].gpio_pin) {
      gpio_init.Pin = capture_hw[k].gpio_pin;
      gpio_init.Alternate = capture_hw[k].af;
      LL_GPIO_Init(GPIOA, &gpio_init);
    }
    LL_TIM_IC_SetActiveInput(capture_hw[k].tim, capture_hw[k].ll_channel,
                             capture_hw[k].active_input);
    LL_TIM_IC_SetPrescaler(capture_hw[k].tim, capture_hw[k].ll_channel, LL_TIM_ICPSC_DIV1);
    LL_TIM_IC_SetFilter(capture_hw[k].tim, capture_hw[k].ll_channel, LL_TIM_IC_FILTER_FDIV1);
    LL_TIM_IC_SetPolarity(capture_hw[k].tim, capture_hw[k].ll_channel, capture_hw[k].ic_polarity);
  }

  // Per in-use capture channel: its own circular-DMA channel via the RULOS DMA abstraction (HT/TC
  // callbacks fire at each half-buffer boundary so captures are processed while the other half
  // fills), feeding the owning channel's rising or falling sub-stream.
  for (int k = 0; k < NUM_HW_CAPTURE; k++) {
    channel_t *ch = &channels[capture_hw[k].channel];
    uint8_t s = capture_hw[k].sub;
    ch->has_hw = true;
    ch->sub[s].buf = capture_buf[k];
    ch->sub[s].counter_tag = ((uint32_t)capture_hw[k].channel << COUNTER_CHAN_SHIFT) |
                             ((s == SUB_RISING) ? COUNTER_POLARITY_BIT : 0);
    sub_ctx[k].chan = ch;
    sub_ctx[k].sub = s;

    const rulos_dma_config_t cfg = {
        .request = capture_hw[k].request,
        .direction = RULOS_DMA_DIR_PERIPH_TO_MEM,
        .mode = RULOS_DMA_MODE_CIRCULAR,
        .periph_width = RULOS_DMA_WIDTH_WORD,
        .mem_width = RULOS_DMA_WIDTH_WORD,
        .periph_increment = false,
        .mem_increment = true,
        .priority = RULOS_DMA_PRIORITY_VERYHIGH,
        .tc_callback = on_dma,
        .ht_callback = on_dma,
        .user_data = &sub_ctx[k],
    };
    ch->sub[s].dma_ch = rulos_dma_alloc(&cfg);
    if (ch->sub[s].dma_ch == NULL) {
      __builtin_trap();
    }
    rulos_dma_start(ch->sub[s].dma_ch, (volatile void *)capture_hw[k].ccr, (void *)capture_buf[k],
                    DMA_CAPTURE_BUFLEN);
    SET_BIT(capture_hw[k].tim->DIER, capture_hw[k].dier_bit);
  }

  // Apply each channel's persisted (or default) slope and divider -- arming the slope-selected
  // captures and programming the divider's prescaler split -- then start the timer pair. Both
  // counters preset to 0; enabling TIM2 (the master) fires its ENABLE-sourced TRGO, which starts
  // TIM5 on the same clock edge.
  for (int i = 0; i < NUM_CHANNELS; i++) {
    apply_channel_config(i, channels[i].slope);
  }
  LL_TIM_SetCounter(TIM2, 0);
  LL_TIM_SetCounter(TIM5, 0);
  LL_TIM_EnableCounter(TIM2);

  // Configure TIM15: fires twice per TIM2 rollover to increment the high bits of the counter.
  __HAL_RCC_TIM15_CLK_ENABLE();
  HAL_NVIC_SetPriority(TIM15_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(TIM15_IRQn);
#define SMALLCOUNTER_MAX 10000
  LL_TIM_InitTypeDef timer15_init = {
      // The main counter counts to the clock frequency (250MHz), but TIM15 is only a 16-bit counter
      // which can't count that high. We scale everything down so it counts up to 10,000 in the time
      // it takes the main counter counts to 250M.
      .Prescaler = (CLOCK_FREQ_HZ / SMALLCOUNTER_MAX) - 1,
      .CounterMode = LL_TIM_COUNTERMODE_UP,

      // Set the autoreload to half the 16B count so this timer fires twice per rollover of the main
      // counter.
      .Autoreload = (SMALLCOUNTER_MAX / 2) - 1,
      .ClockDivision = LL_TIM_CLOCKDIVISION_DIV1,
      .RepetitionCounter = 0,
  };
  LL_TIM_Init(TIM15, &timer15_init);
  LL_TIM_DisableMasterSlaveMode(TIM15);
  LL_TIM_ClearFlag_UPDATE(TIM15);
  LL_TIM_EnableIT_UPDATE(TIM15);

  // Start the counter one-quarter of the way through the full-counter rollover period so that this
  // one fires 1/4 and 3/4 of the way through the main counter.
  LL_TIM_SetCounter(TIM15, SMALLCOUNTER_MAX / 4);
  LL_TIM_EnableCounter(TIM15);
}

void timestamper_set_slope(int ch, timestamper_slope_t slope) {
  if (ch < 0 || ch >= NUM_CHANNELS) {
    return;
  }
  channel_t *c = &channels[ch];
  if (!c->has_hw || c->slope == slope) {
    c->slope = slope;
    return;
  }
  // The slope also decides the divider's hardware/software split, so the full reconfiguration
  // sequence applies.
  apply_channel_config(ch, slope);
}

timestamper_slope_t timestamper_get_slope(int ch) {
  if (ch < 0 || ch >= NUM_CHANNELS) {
    return TIMESTAMPER_SLOPE_RISING;
  }
  return channels[ch].slope;
}

void timestamper_set_divider(int ch, uint32_t n) {
  if (ch < 0 || ch >= NUM_CHANNELS || n == 0) {
    return;
  }
  channel_t *c = &channels[ch];
  c->divider_set = n;
  if (!c->has_hw) {
    c->divider = n;
    c->count = 0;
    return;
  }
  // Reprogram the hardware/software split. The sequence resets both progress counts (software and
  // the prescaler's), so the next group starts fresh at the new divider.
  apply_channel_config(ch, c->slope);
}

uint32_t timestamper_get_divider(int ch) {
  if (ch < 0 || ch >= NUM_CHANNELS) {
    return 1;
  }
  return channels[ch].divider_set;
}

void timestamper_set_stream_enabled(bool enabled) {
  stream_enabled = enabled;
  // Restart the TX pump immediately on enable. The send chain is completion-driven and dies
  // whenever it finds the stream disabled (e.g. after emitting the clear marker, since hosts
  // re-enable only AFTER reading it); without a kick here the first send waits for the 100 ms
  // periodic fallback, during which a 100 kHz input fills the ~164 ms ring and then drops pulses.
  if (enabled) {
    fill_tx_buf();
    try_send_timestamps();
  }
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

// --- Persisted channel config (slope + divider) ------------------- channels[] is authoritative;
// flash is its mirror. Slope/divider changes only touch RAM -- flash is written solely by the
// explicit CONFig:SAVE command (and *RST, persisting defaults). A flash erase/program on this
// single-bank part stalls instruction fetch for tens of ms; doing that on the streaming hot path
// corrupts capture and risks a fault-reset mid-write, so a save instead masks interrupts and
// accepts that pulses during it are lost.

// Persisted payload: per-channel slope + divider. Bump the version whenever this layout changes so
// an older firmware's block is rejected (defaults used) rather than misread.
#define TS_CFG_VERSION 1

typedef struct {
  uint8_t slope[NUM_CHANNELS];
  uint32_t divider[NUM_CHANNELS];
} ts_persist_t;

// Mirror of what's currently in flash, so a save can skip a redundant erase+program when nothing
// actually changed.
static ts_persist_t cfg_last_saved;
static bool cfg_last_saved_valid;

static void cfg_snapshot(ts_persist_t *c) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    c->slope[i] = (uint8_t)channels[i].slope;
    c->divider[i] = channels[i].divider_set;
  }
}

void timestamper_config_save(void) {
  ts_persist_t c;
  cfg_snapshot(&c);
  if (cfg_last_saved_valid && memcmp(&c, &cfg_last_saved, sizeof(c)) == 0) {
    return;  // unchanged -- no flash write
  }
  // Mask interrupts across the flash erase/program: nothing else may run from flash while it's
  // busy. Capture/stream ISRs are silenced for the duration -- pulses arriving now are lost, by
  // design.
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
  timestamper_serial_set_enabled(false);
  timestamper_serial_set_baud(SERIAL_DEFAULT_BAUD);
  timestamper_discard_pending();

  // Full factory reset: persist the defaults now (interrupt-masked, same as CONFig:SAVE).
  timestamper_config_save();
}

void timestamper_discard_pending(void) {
  // ts_head is touched by the DMA ISR and the periodic flush; pause interrupts briefly so we don't
  // race a half-written entry. The in-flight USB TX half also gets reset so any partially-formatted
  // record is dropped. The marker flag is set inside the same critical section so it can't be
  // observed before the ring is empty.
  __disable_irq();
  ts_head = 0;
  ts_tail = 0;
  tx_fill_pos = 0;
  serial_line_head = 0;
  serial_line_tail = 0;
  serial_drops = 0;
  for (int i = 0; i < NUM_CHANNELS; i++) {
    channels[i].num_missed = 0;
    channels[i].buf_overflows = 0;
    channels[i].count = 0;
    if (!channels[i].has_hw) {
      continue;
    }
    // Skip past everything captured so far: the next edge becomes the first one the host sees.
    for (uint8_t s = 0; s < NUM_SUBS; s++) {
      channels[i].sub[s].drain_pos = safe_cur(&channels[i], s);
    }
  }
  // Clear any pending overcapture flags along with the counters they feed: a CCxOF latched before
  // this discard would otherwise be sampled by the next periodic flush and reported as in-window
  // loss for an event the host just discarded.
  for (int k = 0; k < NUM_HW_CAPTURE; k++) {
    capture_hw[k].tim->SR = ~capture_hw[k].sr_of_bit;  // rc_w0
  }
  marker_pending = true;
  __enable_irq();
  // Emit the marker now (or, if a transfer is in flight, arm its completion to do so) rather than
  // waiting up to 100 ms for the periodic fallback: the host can't re-enable the stream until it
  // sees the marker, and the ring is filling the whole time.
  try_send_timestamps();
}

int main() {
  rulos_hal_init();

  // initialize scheduler with 10msec jiffy clock
  init_clock(10000, TIMER1);

  // Initial channel state. Slope and divider are runtime-configurable via SCPI; defaults match what
  // *RST also restores.
  memset(&channels, 0, sizeof(channels));
  for (int i = 0; i < NUM_CHANNELS; i++) {
    channels[i].divider_set = 1;
    channels[i].divider = 1;
    channels[i].slope = TIMESTAMPER_SLOPE_RISING;
  }

  // Override the compiled defaults with the persisted config if a valid block is in flash.
  // rulos_hal_init() has mapped flash for reads; init_timers() below applies channels[] to the
  // hardware.
  ts_persist_t saved;
  if (nvconfig_load(&saved, sizeof(saved), TS_CFG_VERSION)) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
      channels[i].slope = (timestamper_slope_t)saved.slope[i];
      channels[i].divider_set = saved.divider[i] ? saved.divider[i] : 1;
      channels[i].divider = channels[i].divider_set;
    }
  }
  // Seed the saved-mirror with what's now in channels[] (loaded block or compiled defaults) so the
  // first config command doesn't rewrite an identical block.
  cfg_snapshot(&cfg_last_saved);
  cfg_last_saved_valid = true;

  // initialize uart for debug logging
  uart_init(&uart, /*uart_id=*/0, SERIAL_DEBUG_BAUD);
  log_bind_uart(&uart);

  // initialize the output LEDs
  led_chan_pins[0] = LED_CHAN0;
  led_chan_pins[1] = LED_CHAN1;
  led_chan_pins[2] = LED_CHAN2;
  led_chan_pins[3] = LED_CHAN3;
#ifdef LED_CLOCK
  gpio_make_output(LED_CLOCK);
  gpio_set_speed(LED_CLOCK, GPIO_OSPEED_LOW);
#endif
  gpio_make_output(LED_USB);
  gpio_set_speed(LED_USB, GPIO_OSPEED_LOW);
  for (int i = LED_CHAN_FIRST; i < NUM_CHANNELS; i++) {
    gpio_make_output(led_chan_pins[i]);
    gpio_set_speed(led_chan_pins[i], GPIO_OSPEED_LOW);
  }

  // Initialise the SCPI front-end. This also brings up the USB CDC connection used both for
  // incoming SCPI commands and for outgoing timestamp streams.
  timestamper_scpi_init(on_usb_tx_complete);

  // initialize the main timer and its input capture pin
  init_timers();

  schedule_us(1, periodic_task, NULL);
  scheduler_run();
}
