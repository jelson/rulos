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

// Cross-file interface between timestamper.c (which owns the channel
// state and the TIM2 hardware) and scpi.c (which translates SCPI
// commands into calls on this API). Mirrors the pulsegen.h pattern.

#include <stdbool.h>
#include <stdint.h>

#define NUM_CHANNELS 4

// Per-channel slope. Updates hardware (TIM2 input-capture polarity) in
// addition to stashed state.
typedef enum {
  TIMESTAMPER_SLOPE_RISING,   // capture LOW->HIGH transitions only
  TIMESTAMPER_SLOPE_FALLING,  // capture HIGH->LOW transitions only
  TIMESTAMPER_SLOPE_BOTH,     // capture both transitions
} timestamper_slope_t;

void timestamper_set_slope(int ch, timestamper_slope_t slope);
timestamper_slope_t timestamper_get_slope(int ch);

// Per-channel divider: report only every Nth pulse. N=1 reports every
// pulse. N=0 is rejected. The divider count is reset to 0 on every set
// so the next pulse starts a fresh group.
void timestamper_set_divider(int ch, uint32_t n);
uint32_t timestamper_get_divider(int ch);

// Continuous timestamp output enable. When disabled, hardware capture
// keeps running and the internal buffer keeps filling, but nothing
// streams to USB and missed-pulse reports are suppressed -- so a host
// can issue SCPI queries and receive clean, uncluttered responses.
// Default true at boot.
void timestamper_set_stream_enabled(bool enabled);
bool timestamper_get_stream_enabled(void);

// *RST: restore all channels to defaults (rising-edge, divider=1) and
// re-enable streaming.
void timestamper_reset_all(void);

// Identity string returned for *IDN?.
extern const char *const timestamper_idn;
