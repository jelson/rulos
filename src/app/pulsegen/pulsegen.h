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

#define PULSEGEN_FW_VERSION "1.0.0"

// Setters for each SCPI-controllable parameter. Each stashes the new value
// and rebuilds the HRTIM configuration. Returns NULL on success, or a static
// SCPI error string (e.g. `"-200,\"Period out of range\""`) describing why
// the targeted channel is invalid.
//
// period applies to the channel's whole domain: just that channel in async
// mode (each channel is fully independent, on its own HRTIM and GP timers),
// all four channels in sync mode. width and delay (rising-edge offset) are
// per channel.
const char *pulsegen_set_state(int ch, bool on);
const char *pulsegen_set_period_ps(int ch, uint64_t ps);
const char *pulsegen_set_width_ps(int ch, uint64_t ps);
const char *pulsegen_set_delay_ps(int ch, uint64_t ps);

// Burst parameters apply to the channel's whole domain, like period. With
// burst on, the domain emits exactly ncyc pulses (1..65534), rests low, and
// repeats every interval. The count and intra-burst spacing are hardware-exact
// (the HRTIM burst controller in the fast regime, a software per-period count
// in the slow regime); the repetition interval is timed by a hardware timer,
// so it is jitter-free. Only one domain can burst at a time in async mode;
// sync mode bursts all four channels coherently.
//
// Disabling a live burst also disables the domain's outputs (re-enable
// them explicitly): the domain period is the intra-burst spacing, so
// running on after burst-off would stream continuously at the burst's
// instantaneous rate.
const char *pulsegen_set_burst_state(int ch, bool on);
const char *pulsegen_set_burst_ncyc(int ch, uint32_t ncyc);
const char *pulsegen_set_burst_period_ps(int ch, uint64_t ps);

// Query accessors for the burst settings; each reads back the channel's
// domain-effective value.
bool pulsegen_get_burst_state(int ch);
uint32_t pulsegen_get_burst_ncyc(int ch);
uint64_t pulsegen_get_burst_period_ps(int ch);

// Select ASYNC (four independent channels) or SYNC (all channels share
// one period, phase-locked). Returns NULL (never fails).
const char *pulsegen_set_mode(bool sync);

// "ASYNC" or "SYNC", for the MODE? query.
const char *pulsegen_mode_str(void);

// *RST: ASYNC mode, all outputs off, all per-channel config cleared; burst
// off, burst count 1, burst interval 1 s.
void pulsegen_reset_all(void);
