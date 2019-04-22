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

#pragma once

#include "core/rulos.h"

// These are the hardware-specific functions that must be implemented
// by any platform that supports I2S.

typedef void (*hal_i2s_play_done_cb_t)(void* user_data, uint16_t *samples);

void hal_i2s_init(uint16_t sample_rate,
		  hal_i2s_play_done_cb_t play_done_cb, void* user_data);
void hal_i2s_play(uint16_t* samples, uint16_t num_samples);
