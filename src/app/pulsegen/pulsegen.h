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

#define NUM_CHANNELS 4

#define PULSEGEN_FW_VERSION "0.1.0"

// Setters for each SCPI-controllable parameter. Each stashes the new value
// and rebuilds the HRTIM configuration. Returns NULL on success, or a static
// SCPI error string (e.g. `"-200,\"Period out of range\""`) describing why
// the targeted channel is invalid.
//
// period applies to the channel's whole domain: its group (the other channel
// on the same HRTIM timer) in async mode, all four channels in sync mode.
// width and delay (rising-edge offset) are per channel.
const char *pulsegen_set_state(int ch, bool on);
const char *pulsegen_set_period_ps(int ch, uint64_t ps);
const char *pulsegen_set_width_ps(int ch, uint64_t ps);
const char *pulsegen_set_delay_ps(int ch, uint64_t ps);

// Select ASYNC (two independent frequency groups) or SYNC (all channels share
// one period, phase-locked). Returns NULL (never fails).
const char *pulsegen_set_mode(bool sync);

// "ASYNC" or "SYNC", for the MODE? query.
const char *pulsegen_mode_str(void);

// *RST: ASYNC mode, all outputs off, all per-channel config cleared.
void pulsegen_reset_all(void);
