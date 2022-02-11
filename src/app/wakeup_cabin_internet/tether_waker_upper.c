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

#include "core/rulos.h"
#include "periph/uart/uart.h"
//#include "graveyard/spiflash.h"

#include "core/hardware.h"  // sorry. Blinky LED.

//////////////////////////////////////////////////////////////////////////////

typedef struct {
  int cport;  // Wish I could take a handle to the GPIO typedef! We should do
              // that.
  uint16_t on_time_ms;
  uint16_t off_time_ms;
  int state;
} BlinkAct;

void blink_update(BlinkAct *ba) {
  ba->state = (ba->state == 0);
  uint32_t next_delay_us =
      ((uint32_t)(ba->state ? ba->on_time_ms : ba->off_time_ms)) *
      1000 /* ms->us */;
  if (ba->cport == 3) {
    gpio_set_or_clr(GPIO_C3, (ba->state == 0));
  } else if (ba->cport == 4) {
    gpio_set_or_clr(GPIO_C4, (ba->state == 0));
  } else if (ba->cport == 5) {
    gpio_set_or_clr(GPIO_C5, (ba->state == 0));
  }
  schedule_us(next_delay_us, (ActivationFuncPtr)blink_update, ba);
}

void blink_init(BlinkAct *ba, int cport, uint16_t on_time_ms,
                uint16_t off_time_ms) {
  ba->cport = cport;
  ba->on_time_ms = on_time_ms;
  ba->off_time_ms = off_time_ms;
  if (ba->cport == 3) {
    gpio_make_output(GPIO_C3);
  } else if (ba->cport == 4) {
    gpio_make_output(GPIO_C4);
  } else if (ba->cport == 5) {
    gpio_make_output(GPIO_C5);
  }
  schedule_us(50000, (ActivationFuncPtr)blink_update, ba);
}

//////////////////////////////////////////////////////////////////////////////

char reply_buf[80];
BlinkAct blink5;
BlinkAct blink4;
BlinkAct blink3;

int main() {
  rulos_hal_init();

  // start clock with 10 msec resolution
  init_clock(10000, TIMER1);

  blink_init(&blink5, 5, 250, 750);
  blink_init(&blink4, 4, 3000, 7000);
  blink_init(&blink3, 3, 3000, 7000);

  scheduler_run();
}
