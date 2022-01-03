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
 * Basic test program that frobs a GPIO; should compile for all platforms. No
 * peripherals. Meant as a basic test of the build system for new platforms and
 * chips.
 */

#include "core/hardware.h"
#include "core/rulos.h"
#include "soc/timer_group_struct.h"

#define TEST_PIN GPIO_2

void print_one_timergroup(timg_dev_t *dev, const char *label) {
  for (int i = 0; i < 2; i++) {
    uint32_t ena = dev->hw_timer[i].config.enable;
    uint32_t al_en = dev->hw_timer[i].config.alarm_en;
    dev->hw_timer[i].update = 1;
    uint32_t val = dev->hw_timer[i].cnt_low;
    LOG("%s, timer %d: ", label, i);
    LOG("  enabled: %d", ena);
    LOG("  alarm enabled: %d", al_en);
    LOG("  value: %d", val);
  }
}

static void print_timer_info(void) {
  LOG("printing timer info");
  print_one_timergroup(&TIMERG0, "TIMERG0");
  print_one_timergroup(&TIMERG1, "TIMERG1");
}

static volatile int ints_received = 0;

static void tick(void *arg) {
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  ints_received++;
}

int main() {
  UartState_t uart;
  rulos_hal_init();
  uart_init(&uart, /* uart_id= */ 0, 38400);
  log_bind_uart(&uart);

  LOG("Log output running!");

  gpio_make_output(TEST_PIN);
  uint32_t period = hal_start_clock_us(1000000, tick, NULL, TIMER0);
  LOG("Started clock, period of %d", period);
  print_timer_info();
  int ints_printed = 0;
  while (true) {
    if (ints_printed != ints_received) {
      LOG("%d interrupts received", ints_received);
      ints_printed = ints_received;
      print_timer_info();
    }
    hal_idle();
  }
}
