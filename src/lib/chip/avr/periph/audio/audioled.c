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

#define AUDIO_LED_RED GPIO_D2
#define AUDIO_LED_YELLOW GPIO_D3

void audioled_init() {
  gpio_make_output(AUDIO_LED_RED);
  gpio_make_output(AUDIO_LED_YELLOW);
}

void audioled_set(bool red, bool yellow) {
  gpio_set_or_clr(AUDIO_LED_RED, !red);
  gpio_set_or_clr(AUDIO_LED_YELLOW, !yellow);
}
