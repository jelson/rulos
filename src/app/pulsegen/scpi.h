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

/*
 * SCPI front-end for pulsegen. Owns the USB CDC port, the linereader that
 * splits incoming bytes into lines, and the parser that dispatches each line.
 *
 * Supported commands (long and short forms):
 *   *IDN?
 *   *RST
 *   SYSTem:ERRor?
 *   OUTPut[n]:STATe ON|OFF|1|0
 *   SOURce[n]:PULSe:PERiod  <seconds>
 *   SOURce[n]:PULSe:WIDTh   <seconds>
 *   SOURce[n]:PULSe:COUNt   <integer>
 *   SOURce[n]:BURSt:PERiod  <seconds>
 *
 * Hardware mutation happens in pulsegen.c via the pulsegen_* setters
 * declared in pulsegen.h.
 */

#include <stdbool.h>

// One-time setup: USB CDC + linereader. Call after rulos_hal_init().
void scpi_init(void);

// Send a single line to the host (USB CDC). Newline is appended. No-op if
// USB is not enumerated or a previous transmission is still in flight, so
// callers should only emit lines they are willing to drop.
void scpi_print(const char *line);

// True iff USB CDC has finished enumeration.
bool scpi_usb_ready(void);

// One-shot: returns true if any RX or TX traffic has happened since the
// last call. Used by the LED blinker.
bool scpi_consume_recent_activity(void);
