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

// Per-channel slope. Both edges are always captured in hardware (a
// permanently rising-armed and a permanently falling-armed TIM2
// capture channel per input); slope selects which are emitted. Every
// emitted record carries an exact, hardware-latched polarity bit
// regardless of slope.
//
// Emission order: each (channel, polarity) sub-stream is emitted in
// time order, but sub-streams are not interleaved into one globally
// ordered sequence -- in BOTH mode a channel's rising and falling
// records arrive in independent service-sized batches, just as
// records from different channels always have. Hosts that want total
// order sort by timestamp.
typedef enum {
  TIMESTAMPER_SLOPE_RISING,   // emit LOW->HIGH transitions only
  TIMESTAMPER_SLOPE_FALLING,  // emit HIGH->LOW transitions only
  TIMESTAMPER_SLOPE_BOTH,     // emit both transitions
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
// "<chan> <sec>.<ns> <+|->\n" per timestamp, where the final column
// is the edge polarity: '+' rising, '-' falling); BINARY emits a
// fixed TIMESTAMPER_BINARY_RECORD_LEN-byte record per timestamp --
// the same 8-byte representation used in the device's internal ring
// buffer:
//   bytes 0-3: seconds (uint32 LE)
//   bytes 4-7: counter (uint32 LE): bits [31:30] channel
//              (0..NUM_CHANNELS-1), [29] edge polarity (1 = rising
//              '+', 0 = falling '-'), [27:0] raw tick count
//              (0..249,999,999 at 250 MHz, i.e. 4 ns per tick).
//              For normal timestamp records, bits [31:29] are a
//              direct channel/edge index:
//              counter >> 29 == channel * 2 + (rising ? 1 : 0).
// Every record is exactly 8 bytes, timestamp or not. Counter bit [28]
// marks a "special message" instead of a timestamp: device metadata
// that would be a "#" comment line in text mode, carried in-band so
// binary readers see it too. When set, counter [7:0] is the message
// type, bit [29] is zero, and the seconds word is its payload:
//   0 OUTPUT_CLEARED -- payload all-zero (still a full 8-byte record);
//                       binary analog of the "# output cleared" marker.
//   1 PULSES_LOST    -- counter [31:30] = channel; seconds [15:0] =
//                       overcaptures, [31:16] = buffer overflows
//                       (each saturated at 65535).
//   2 OSC_FAIL       -- payload all-zero; the reference clock failed
//                       and the device has halted (binary analog of
//                       the "# FATAL ... oscillator" line).
// Text mode is unchanged: same "#" comment lines as before.
typedef enum {
  TIMESTAMPER_FORMAT_TEXT,
  TIMESTAMPER_FORMAT_BINARY,
} timestamper_format_t;
#define TIMESTAMPER_BINARY_RECORD_LEN 8

void timestamper_set_format(timestamper_format_t fmt);
timestamper_format_t timestamper_get_format(void);

// *RST: restore all channels to defaults (rising-edge, divider=1),
// switch back to TEXT format, re-enable streaming, and persist the
// defaults to flash (a full factory reset that survives power loss).
void timestamper_reset_all(void);

// Persist the current channel config (slope + divider) to flash so it
// survives power loss. Synchronous and DESTRUCTIVE to sampling: it
// masks interrupts for the flash erase/program (tens of ms), so pulses
// arriving during the save are missed -- by design. Driven by the
// explicit CONFig:SAVE command (and *RST, which persists defaults).
// A change that matches what's already stored is skipped.
void timestamper_config_save(void);

// Discard everything currently pending: drop the timestamp ring,
// the in-flight TX-buffer half, and the per-channel missed/overflow
// counters. After this returns, the next captured pulse will be the
// first one the host sees. Channel configuration (slope, divider,
// format, stream enable) is preserved.
void timestamper_discard_pending(void);

#define TIMESTAMPER_FW_VERSION "0.16.0"
