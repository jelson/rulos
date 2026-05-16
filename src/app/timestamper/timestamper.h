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

// Output stream format. TEXT is the documented default (ASCII
// "<chan> <sec>.<ns>\n" per timestamp); BINARY emits a fixed
// TIMESTAMPER_BINARY_RECORD_LEN-byte record per timestamp -- the same
// 8-byte representation used in the device's internal ring buffer:
//   bytes 0-3: seconds (uint32 LE)
//   bytes 4-7: counter (uint32 LE), where the top 2 bits are the
//              channel (0..NUM_CHANNELS-1) and the low 30 bits are
//              the raw tick count (0..249,999,999 at 250 MHz, i.e.
//              4 ns per tick).
// Comments / overcapture / overflow lines are suppressed in binary
// mode (the host should switch back to TEXT to read them).
typedef enum {
  TIMESTAMPER_FORMAT_TEXT,
  TIMESTAMPER_FORMAT_BINARY,
} timestamper_format_t;
#define TIMESTAMPER_BINARY_RECORD_LEN 8

void timestamper_set_format(timestamper_format_t fmt);
timestamper_format_t timestamper_get_format(void);

// *RST: restore all channels to defaults (rising-edge, divider=1),
// switch back to TEXT format, and re-enable streaming.
void timestamper_reset_all(void);

// Discard everything currently pending: drop the timestamp ring,
// the in-flight TX-buffer half, and the per-channel missed/overflow
// counters. After this returns, the next captured pulse will be the
// first one the host sees. Channel configuration (slope, divider,
// format, stream enable) is preserved.
void timestamper_discard_pending(void);

#define TIMESTAMPER_FW_VERSION "0.9.0"

// Identity string returned for *IDN?.
extern const char *const timestamper_idn;
