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
 */

// Silent RX-drop detector. Uses linereader to receive fixed-length
// lines (length established by the first line received) and counts
// mismatches. No per-line output, so TX backpressure can't induce
// RX drops; a single periodic summary is the only output.
//
// Usage:
//   1. Flash this binary to the target and connect a terminal to the
//      console UART (uart 0, 1 Mbps).
//   2. Run write.py to generate fixed-length padded lines:
//        ./write.py <port> <num_lines>
//   3. Watch the firmware's periodic summary:
//        lines=N expected_len=L length_mismatches=M
//        linereader_overflows=O bps=B
//      No drops iff lines == num_lines sent, length_mismatches == 0,
//      and linereader_overflows == 0. bps is averaged from the first
//      line received to the most recent.

#include <string.h>

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/uart/linereader.h"
#include "periph/uart/uart.h"

UartState_t console;
LineReader_t lr;

static uint32_t expected_len = 0;
static uint32_t lines_received = 0;
static uint32_t length_mismatches = 0;
static uint32_t total_bytes = 0;
static Time first_line_time = 0;
static Time last_line_time = 0;

static void line_received(UartState_t *uart, void *user_data, char *line) {
  size_t len = strlen(line);
  lines_received++;
  total_bytes += len + 1;  // +1 for the \n that was on the wire
  last_line_time = precise_clock_time_us();
  if (expected_len == 0) {
    expected_len = len;
    first_line_time = last_line_time;
  } else if (len != expected_len) {
    length_mismatches++;
  }
}

#define REPORT_PERIOD_USEC 1000000
static uint32_t last_reported_lines = 0;

static void report(void *data) {
  schedule_us(REPORT_PERIOD_USEC, report, NULL);
  if (lines_received == last_reported_lines) {
    return;
  }
  last_reported_lines = lines_received;

  // bps averaged across the whole run (first line to most recent).
  Time elapsed_us = last_line_time - first_line_time;
  uint32_t bps = 0;
  if (elapsed_us > 0) {
    bps = (uint32_t)(((uint64_t)total_bytes * 8 * 1000000) / elapsed_us);
  }

  LOG("lines=%" PRIu32 " expected_len=%" PRIu32
      " length_mismatches=%" PRIu32 " linereader_overflows=%" PRIu32
      " bps=%" PRIu32,
      lines_received, expected_len, length_mismatches, lr.overflows, bps);
}

int main() {
  rulos_hal_init();
  uart_init(&console, /* uart_id= */ 0, 1000000);
  log_bind_uart(&console);
  LOG("linecounter up and running");

  linereader_init(&lr, &console, line_received, NULL);

  init_clock(10000, TIMER1);
  schedule_us(REPORT_PERIOD_USEC, report, NULL);
  scheduler_run();
}
