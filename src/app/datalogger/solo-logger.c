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
#include "flash_dumper.h"
#include "periph/uart/uart.h"
#include "serial_reader.h"

// uart definitions
#define CONSOLE_UART_NUM 0
#define PSOC_TX_UART_NUM 1

#define GREEN_LED  GPIO_A11
#define ORANGE_LED GPIO_A12

UartState_t console;
flash_dumper_t flash_dumper;
serial_reader_t psoc_console_tx;

static void turn_off_led(void *data) {
  gpio_clr(GREEN_LED);
  gpio_clr(ORANGE_LED);
}

static void indicate_alive(void *data) {
  schedule_us(30000000, indicate_alive, NULL);
  if (flash_dumper.ok && serial_reader_is_active(&psoc_console_tx)) {
    gpio_set(ORANGE_LED);
  } else {
    gpio_set(GREEN_LED);
  }
  schedule_us(3000, turn_off_led, NULL);
}

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER1);

  // initialize console uart
  uart_init(&console, CONSOLE_UART_NUM, 1000000);
  log_bind_uart(&console);
  LOG("SoloListener starting, rev " STRINGIFY(GIT_COMMIT));

  // initialize flash dumper
  flash_dumper_init(&flash_dumper);

  // initialize serial readers
  serial_reader_init(&psoc_console_tx, PSOC_TX_UART_NUM, 1000000, &flash_dumper,
                     NULL, NULL);

  // enable periodic blink to indicate liveness
  schedule_now(indicate_alive, NULL);

  gpio_make_output(ORANGE_LED);
  gpio_make_output(GREEN_LED);

  scheduler_run();
  return 0;
}
