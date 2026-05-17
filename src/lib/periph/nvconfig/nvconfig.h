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

// Persist a small opaque config blob across power loss.
//
// The blob is stored, framed by a magic / version / length / CRC32
// header, in a flash region the app reserves at link time. Reserve it
// by passing flash_reserve_k=<KB> to ArmStmPlatform in the app's
// SConstruct (default 0 = no region; stm32-generic.ld then exposes the
// region via _nvconfig_base and link-asserts that code can't overrun
// it). Request the peripheral by adding "nvconfig" to `peripherals`.
//
// The payload is opaque to this module: the caller passes its own
// struct plus a version it bumps whenever that struct's layout
// changes, so a stale block from an older firmware is rejected rather
// than misinterpreted.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Largest payload the framed block (header + payload + CRC, padded to
// the flash program unit) is guaranteed to hold. Far above any plausible
// config; keeps the write image a small fixed static buffer.
#define NVCONFIG_MAX_PAYLOAD 256

// Read the persisted blob into `out` (`len` bytes). Returns true only
// if a block with a valid magic, a matching `version` and `len`, and a
// correct CRC is present. Returns false (leaving `out` untouched) if
// the region is blank, corrupt, or written by a different layout --
// the caller then keeps its compiled defaults.
bool nvconfig_load(void *out, size_t len, uint16_t version);

// Erase the reserved region and write `data` (`len` bytes) with a
// fresh header and CRC. Synchronous: the core stalls on flash-busy for
// the erase+program (tens of ms), briefly interrupting anything the
// app is streaming -- call only while reconfiguring, never on a hot
// path. `len` must be <= NVCONFIG_MAX_PAYLOAD and fit the reserved
// region; an oversize request is a no-op.
void nvconfig_save(const void *data, size_t len, uint16_t version);
