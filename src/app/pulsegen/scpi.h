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
 * Pulsegen SCPI dispatcher. The generic SCPI library
 * (src/lib/periph/scpi) handles USB CDC, the linereader, and the IEEE
 * 488.2 mandatory commands. Use scpi_print, scpi_set_error,
 * scpi_clear_error, scpi_usb_ready, and scpi_consume_recent_activity
 * directly from periph/scpi/scpi.h for runtime interaction.
 *
 * Pulsegen-specific commands handled here:
 *   OUTPut[n]:STATe ON|OFF|1|0
 *   SOURce[n]:PULSe:PERiod  <seconds>
 *   SOURce[n]:PULSe:WIDTh   <seconds>
 *   SOURce[n]:PULSe:COUNt   <integer>
 *   SOURce[n]:BURSt:PERiod  <seconds>
 *
 * Hardware mutation happens in pulsegen.c via the pulsegen_* setters
 * declared in pulsegen.h.
 */

// One-time setup. Call after rulos_hal_init().
void pulsegen_scpi_init(void);
