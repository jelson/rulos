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

#define FREQ_USEC 50000

#if defined(RULOS_ARM_STM32)
#define TEST_PIN GPIO_A0
#elif defined(RULOS_AVR)
#define TEST_PIN GPIO_B3
#else
#error "No test pin defined"
#endif

UartState_t uart;

void test_func(void *data) {
  schedule_us(FREQ_USEC, (ActivationFuncPtr)test_func, NULL);

  // First send a message short enough to fit into the RULOS 128-byte buffer
  // output, so the send returns immediately. Then send a longer message that
  // does not fit into the buffer.
  //
  // Using the GPIO output observed by a scope (or just an internal timer, if I
  // were not lazy), we can verify that the initial buffered send returns
  // immediately.
  //
  // The second send verifies that messages too long to fit into the buffer are
  // still output in their entirely, correctly.
  gpio_set(TEST_PIN);
  uart_print(&uart, "this is a relatively short message; it should "
             "return almost immediately.\n");
  gpio_clr(TEST_PIN);

  static int i = 0;
  gpio_set(TEST_PIN);
  LOG("this is message number %d to test the log macro", i++);
  gpio_clr(TEST_PIN);

  gpio_set(TEST_PIN);
  uart_print(
      &uart,
      "hello there this is an extremely long message, one that actually\n"
      "exceeds the send buffer size of 128 bytes. why would you want to send\n"
      "a message this long? who knows. i don't judge. i just transmit!\n"
      "hopefully, the entire thing has been received!\n\n");
  gpio_clr(TEST_PIN);

  gpio_set(TEST_PIN);
  uart_flush(&uart);
  gpio_clr(TEST_PIN);
}

int main() {
  hal_init();

  uart_init(&uart, /* uart_id= */ 0, 38400, true);
  log_bind_uart(&uart);
  LOG("Log output running");
  LOG("Even more log output running!");

  init_clock(10000, TIMER1);
  gpio_make_output(TEST_PIN);

  schedule_now((ActivationFuncPtr)test_func, NULL);

  cpumon_main_loop();
}
