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

#include "chip/avr/periph/usi_twi_slave/usi_twi_slave.h"
#include "core/hal.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "custom_board_defs.h"

/*
 * This callback is called every time we, as a TWI slave, get a query from
 * the TWI master. Each time that happens, we return whatever key is next
 * in the keypad buffer if one is queued, or 0 otherwise.
 */
static uint8_t get_next_char_to_transmit() {
  return hal_read_keybuf();
}

int main() {
  rulos_hal_init();

  // start clock with 10 msec resolution
  init_clock(10000, TIMER1);

  // start scanning the keypad
  hal_init_keypad();

  // Check the OPT pin. If it's open, use the default 7-bit address of
  // 0x32 (dec 50). If it's been jumpered, use 0x75 (dec 114).
  uint8_t gpio_address;
  gpio_make_input_enable_pullup(OPT_PIN);
  if (gpio_is_set(OPT_PIN)) {
    gpio_address = 0x32;
  } else {
    gpio_address = 0x75;
  }

  // set up a TWI slave handler for TWI queries
  usi_twi_slave_init(gpio_address, NULL, get_next_char_to_transmit);

  scheduler_run();
  assert(FALSE);
}
