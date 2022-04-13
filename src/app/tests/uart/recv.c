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

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/uart/uart.h"
#include "periph/uart/uart_hal.h"

#define FREQ_USEC    100000
#define DUT_UART_NUM 1
#define DUT_BAUD     9600

UartState_t console, dut;
uint32_t console_rx_chars = 0, last_console_rx_chars = 0;
uint32_t dut_rx_chars = 0, last_dut_rx_chars = 0;

static void _buf_received(UartState_t *s, void *user_data, char *buf,
                          size_t buflen) {
  if (s == &console) {
    console_rx_chars += buflen;
  } else {
    dut_rx_chars += buflen;
  }
}

void maybe_print(const char *desc, uint32_t *last, uint32_t *now) {
  if (*last == *now) {
    return;
  }
  LOG("%s: %" PRIu32 " (%" PRIu32 " new)", desc, *now, *now - *last);
  *last = *now;
}

void print_stats(void *data) {
  schedule_us(FREQ_USEC, print_stats, NULL);
#ifdef RULOS_ARM_STM32
  hal_uart_log_stats(console.uart_id);
#endif
  maybe_print("console_rx_chars", &last_console_rx_chars, &console_rx_chars);
  maybe_print("dut_rx_chars", &last_dut_rx_chars, &dut_rx_chars);
}

int main() {
  rulos_hal_init();

  uart_init(&console, /* uart_id= */ 0, 1000000);
  log_bind_uart(&console);
  LOG("Log output running");

  uart_start_rx(&console, _buf_received, NULL);

  uart_init(&dut, /* uart_id= */ DUT_UART_NUM, DUT_BAUD);
  uart_start_rx(&dut, _buf_received, NULL);

  init_clock(10000, TIMER1);
  schedule_now(print_stats, NULL);
  scheduler_run();
}
