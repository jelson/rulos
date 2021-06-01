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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "core/queue.h"
#include "core/rulos.h"
#include "periph/7seg_panel/7seg_panel.h"
#include "periph/uart/uart.h"

#ifdef SIM
#include "core/sim.h"
#endif

#define MILLION                     1000000
#define WALLCLOCK_CALLBACK_INTERVAL 10000

typedef struct {
  BoardBuffer bbuf;
  Time last_redraw_time;
  int hour;
  int minute;
  int second;
  int hundredth;

  int unhappy_state;
  int unhappy_timer;

  char uart_q[8];
  uint8_t uart_qlen;
  Time last_reception_us;
  int mins_since_last_sync;

  uint8_t display_flag;
} WallClockActivation_t;

/******************** Display *******************************/

int precision_threshold_times_min[] = {
    1,    // lose hundredths of a sec
    3,    // lose tenths of a sec
    20,   // lose seconds
    1000  // demand resync
};

static void display_clock(WallClockActivation_t *wca) {
  char buf[9];

  // draw the base numbers
  int_to_string2(buf, 2, 1, wca->hour);
  int_to_string2(buf + 2, 2, 2, wca->minute);
  int_to_string2(buf + 4, 2, 2, wca->second);
  int_to_string2(buf + 6, 2, 2, wca->hundredth);

  // delete some if we haven't resynced the clock in a while
  if (wca->mins_since_last_sync > precision_threshold_times_min[0]) {
    buf[7] = ' ';
  }
  if (wca->mins_since_last_sync > precision_threshold_times_min[1]) {
    buf[6] = ' ';
  }
  if (wca->mins_since_last_sync > precision_threshold_times_min[2]) {
    buf[5] = ' ';
    buf[4] = ' ';
  }
  if (wca->mins_since_last_sync > precision_threshold_times_min[3]) {
    wca->hour = -1;
  }

  ascii_to_bitmap_str(wca->bbuf.buffer, 8, buf);
  wca->bbuf.buffer[1] |= SSB_DECIMAL;
  wca->bbuf.buffer[2] |= SSB_DECIMAL;
  wca->bbuf.buffer[3] |= SSB_DECIMAL;
  wca->bbuf.buffer[4] |= SSB_DECIMAL;
  wca->bbuf.buffer[5] |= SSB_DECIMAL;
}

static void display_unhappy(WallClockActivation_t *wca, uint16_t interval_ms) {
  const char *msg[] = {" NEEd ", "  PC  ", "SErIAL", "      ", NULL};

  wca->unhappy_timer += interval_ms;

  if (wca->unhappy_timer > 1500) {
    wca->unhappy_state++;
    wca->unhappy_timer = 0;
  }
  if (msg[wca->unhappy_state] == NULL) {
    wca->unhappy_state = 0;
  }

  ascii_to_bitmap_str(wca->bbuf.buffer, 8, msg[wca->unhappy_state]);
}

/****************  Clock Management *****************************/

#define SECONDS_BETWEEN_PULSES 60

static void calibrate_clock(WallClockActivation_t *wca, uint8_t hour,
                            uint8_t minute, uint8_t second, Time reception_us) {
  // Compute the elapsed time according to our local clock between
  // this packet received and when the previous one was received
  Time reception_diff_us, error;

  if (wca->last_reception_us && reception_us > wca->last_reception_us) {
    reception_diff_us = reception_us - wca->last_reception_us;
    // There were supposed to be 60 seconds between this one and
    // the last.  TODO: Support arbitrary intervals.
    error = SECONDS_BETWEEN_PULSES * MILLION - reception_diff_us;
  } else {
    reception_diff_us = -1;
    error = 0;
  }

  LOG("got pulse at %d, last at %d, diff is %d, error %d", reception_us,
      wca->last_reception_us, reception_diff_us, error);
  wca->last_reception_us = reception_us;
  wca->mins_since_last_sync = 0;

  // Update the wall-clock.  Account for the time between when we
  // received the pulse and the current time.
  Time time_past_pulse = precise_clock_time_us() - reception_us;
  uint8_t extra_seconds = time_past_pulse / 1000000;
  uint8_t extra_hundredths = (time_past_pulse % 1000000) / 10000;
  wca->hour = hour;
  wca->minute = minute;
  wca->second = second + extra_seconds;
  wca->hundredth = extra_hundredths;

  // If it's off by 5 seconds or more, don't use this as a
  // calibration.
  if (error > 5 * MILLION || error < -5 * MILLION) {
    LOG("error too large -- not correcting");
    return;
  }

  // Compute the ratio by which we're off in parts per million and
  // update the clock rate.  Then divide by 2, to give us an
  // exponentially weighted moving average of the clock rate.
  if (error) {
    Time ratio = error / SECONDS_BETWEEN_PULSES / 2;
    hal_speedup_clock_ppm(ratio);
  }
}

/*
 * Advance the display clock by a given number of milliseconds
 */
static void advance_clock(WallClockActivation_t *wca, uint16_t interval_ms) {
  // if we don't have a valid time, don't change the clock
  if (wca->hour < 0)
    return;

  wca->hundredth += ((interval_ms + 5) / 10);

  while (wca->hundredth >= 100) {
    wca->hundredth -= 100;
    wca->second++;
  }
  while (wca->second >= 60) {
    wca->second -= 60;
    wca->minute++;
    wca->mins_since_last_sync++;
  }
  while (wca->minute >= 60) {
    wca->minute -= 60;
    wca->hour++;
  }
  while (wca->hour >= 24) {
    wca->hour -= 24;
  }
}

/******************* main ***********************************/

static uint8_t ascii_digit(uint8_t c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  else
    return 0;
}

// Check the UART to see if there's a valid message.  I'll arbitrarily
// define a protocol: messages are 8 ASCII characters: T123055E
// ("Time 12:30:55 End").  So, we'll check the uart periodically.
// If the buffer doesn't begin with T, we assume a framing error and
// throw the whole buffer away.
//
// Note that the UART timestamping code returns a timestamp only for
// the first character in the queue.

static void parse_uart_message(void *data) {
  WallClockActivation_t *wca = (WallClockActivation_t *)data;
  uint8_t hour, minute, second;

  // Make sure we got 8 characters - if not, this is a bug, the parser shouldn't
  // be scheduled until we have 8
  assert(wca->uart_qlen == 8);

  // Make sure both framing characters are correct
  if (wca->uart_q[0] != 'T' || wca->uart_q[7] != 'E') {
    goto done;
  }

  // Decode the characters
  hour = ascii_digit(wca->uart_q[1]) * 10 + ascii_digit(wca->uart_q[2]);
  minute = ascii_digit(wca->uart_q[3]) * 10 + ascii_digit(wca->uart_q[4]);
  second = ascii_digit(wca->uart_q[5]) * 10 + ascii_digit(wca->uart_q[6]);

  // Update the clock
  calibrate_clock(wca, hour, minute, second, wca->last_reception_us);

  // indicate we got a message
  wca->display_flag = 25;

done:
  wca->uart_qlen = 0;
}

// Interrupt-time callback when the UART receives a character.
//
// I'll arbitrarily define a protocol: messages are 8 ASCII characters: T123055E
// ("Time 12:30:55 End").
//
// If the receive buffer is empty, the character must be 'T' or it is
// ignored. When this leading T is received, we acquire a timestamp that counts
// as the message's reception time.
//
// If the buffer is non-empty, we just accumulate more characters into the
// buffer until there are 8 of them, then schedule the non-interrupt-time
// upcall.
//
static void uart_char_received(UartState_t *uart, void *data, char c) {
  WallClockActivation_t *wca = (WallClockActivation_t *)data;

  // acquire the timestamp first just in case we might need it
  Time now = precise_clock_time_us();

  if (wca->uart_qlen >= 8) {
    LOG("uart buffer full - dropping");
  } else if (wca->uart_qlen == 0) {
    // first character received: enforce it must be a 'T', otherwise drop it,
    // and remember timestamp
    if (c == 'T') {
      wca->uart_q[wca->uart_qlen] = c;
      wca->uart_qlen++;
      wca->last_reception_us = now;
    } else {
      LOG("framing error on uart; dropping '%c'", c);
    }
  } else {
    wca->uart_q[wca->uart_qlen] = c;
    wca->uart_qlen++;

    if (wca->uart_qlen == 8) {
      schedule_now(parse_uart_message, wca);
    }
  }
}

static void update(WallClockActivation_t *wca) {
  // compute how much time has passed since we were last called
  Time now = clock_time_us();
  Time interval_us = now - wca->last_redraw_time;
  wca->last_redraw_time = now;
  uint16_t interval_ms = (interval_us + 500) / 1000;

  // advance the clock by that amount
  advance_clock(wca, interval_ms);

  // display either the time (if the clock has been set)
  // or an unhappy message (if it has not)
  if (wca->hour < 0)
    display_unhappy(wca, interval_ms);
  else
    display_clock(wca);

  if (wca->display_flag) {
    wca->display_flag--;
    wca->bbuf.buffer[7] |= SSB_DECIMAL;
  }

  board_buffer_draw(&wca->bbuf);

  // schedule the next callback
  schedule_us(WALLCLOCK_CALLBACK_INTERVAL, (ActivationFuncPtr)update, wca);
}

int main() {
  hal_init();
  hal_init_rocketpanel();
  board_buffer_module_init();

  // start clock with 10 msec resolution
  init_clock(WALLCLOCK_CALLBACK_INTERVAL, TIMER1);

  // start the uart running at 38.4k baud
  UartState_t uart;
  uart_init(&uart, /* uart_id= */ 0, 38400, TRUE);
  log_bind_uart(&uart);

  // initialize our internal state
  WallClockActivation_t wca;
  memset(&wca, 0, sizeof(wca));
  wca.hour = -1;
  wca.unhappy_timer = 0;
  wca.unhappy_state = 0;
  wca.last_redraw_time = clock_time_us();
  wca.mins_since_last_sync = 0;

  // init the board buffer
  board_buffer_init(&wca.bbuf DBG_BBUF_LABEL("clock"));
  board_buffer_push(&wca.bbuf, 0);

  // set up uart receiver callback
  uart_start_rx(&uart, uart_char_received, &wca);

  // have the callback get called immediately
  schedule_us(1, (ActivationFuncPtr)update, &wca);

  cpumon_main_loop();
  return 0;
}
