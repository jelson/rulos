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

// Functions that scpi.c calls back into pulsegen. The SCPI parser owns no
// hardware state; all hardware mutation happens here.

#include <stdbool.h>
#include <stdint.h>

#define NUM_CHANNELS 2

// Setters for each SCPI-controllable parameter. The setter stashes the new
// value and, if the channel is currently armed, reconfigures hardware.
// Returns NULL on success, or a static SCPI error string (e.g.
// `"-200,\"Period out of range\""`) on failure.
const char *pulsegen_set_state(int ch, bool on);
const char *pulsegen_set_period_ps(int ch, uint64_t ps);
const char *pulsegen_set_width_ps(int ch, uint64_t ps);
const char *pulsegen_set_count(int ch, uint32_t n);
const char *pulsegen_set_burst_period_ps(int ch, uint64_t ps);

// *RST: disable all outputs and clear all per-channel config.
void pulsegen_reset_all(void);

// Identity string returned for *IDN?.
extern const char *const pulsegen_idn;
