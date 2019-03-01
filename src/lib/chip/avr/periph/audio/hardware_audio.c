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

#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay_basic.h>

#include "core/board_defs.h"
#include "core/hal.h"
#include "core/hardware.h"
#include "core/rulos.h"

#ifdef AUDIO_REGISTER_LATCH

void hal_audio_init(void) {
  gpio_make_output(AUDIO_REGISTER_LATCH);
  gpio_make_output(AUDIO_REGISTER_DATA);
  gpio_make_output(AUDIO_REGISTER_SHIFT);
}

#if 0
uint8_t g_audio_register_delay_constant = 1;
void audio_register_delay()
{
	uint8_t delay;
	static volatile int x;
	for (delay=0; delay<g_audio_register_delay_constant; delay++)
	{
		x = x+1;
	}
}
#endif

void hal_audio_fire_latch(void) {
  gpio_set(AUDIO_REGISTER_LATCH);
  gpio_clr(AUDIO_REGISTER_LATCH);
}

void hal_audio_shift_sample(uint8_t sample) {
  uint8_t i;
  for (i = 0; i < 8; i++) {
    gpio_set_or_clr(AUDIO_REGISTER_DATA, sample & 1);
    gpio_set(AUDIO_REGISTER_SHIFT);
    gpio_clr(AUDIO_REGISTER_SHIFT);
    sample = sample >> 1;
  }
}

#endif  // AUDIO_REGISTER_LATCH
