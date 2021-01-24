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

#include "core/bss_canary.h"
#include "core/hardware.h"
#include "core/rulos.h"

#define FREQ_USEC 50000

#if defined(RULOS_ARM_LPC)
#define TEST_PIN GPIO0_08
#elif defined(RULOS_ARM_STM32)
#define TEST_PIN GPIO_A6
#elif defined(RULOS_AVR)
#define TEST_PIN GPIO_B3
#else
#error "No test pin defined"
#endif

#ifdef LOG_TO_SERIAL
HalUart uart;
#endif

void test_func(void *data) {
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);

#ifdef LOG_TO_SERIAL
  hal_uart_sync_send(
      &uart,
      "hello there this is an extremely long message, one that actually "
      "exceeds the send buffer size of 128 bytes. why would you want to send a "
      "message this long? who knows. i don't judge. i just transmit.\n\n");
  static int i = 0;
  LOG("serial output %d emitted", i++);
#endif

  schedule_us(FREQ_USEC, (ActivationFuncPtr)test_func, NULL);
}

int main() {
  hal_init();

#ifdef LOG_TO_SERIAL
  hal_uart_init(&uart, 115200, true, /* uart_id= */ 0);
  LOG("Log output running");
#endif

  init_clock(10000, TIMER1);
  gpio_make_output(TEST_PIN);

  bss_canary_init();

  schedule_now((ActivationFuncPtr)test_func, NULL);

  cpumon_main_loop();
}
