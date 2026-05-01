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
 * Timestamper SCPI dispatcher. The generic SCPI library
 * (src/lib/periph/scpi) handles USB CDC, the linereader, and the IEEE
 * 488.2 mandatory commands (*IDN?, *RST, *CLS, SYSTem:ERRor?). This
 * file dispatches the timestamper-specific commands.
 *
 * Supported app-specific commands (long and short forms, case-insensitive):
 *
 *   INPut[n]:SLOPe POSitive|NEGative   set rising/falling-edge capture
 *   INPut[n]:SLOPe?                    query
 *   INPut[n]:DIVider <N>               report only every Nth pulse
 *   INPut[n]:DIVider?                  query
 *
 * Hardware mutation happens in timestamper.c via the timestamper_*
 * setters declared in timestamper.h.
 */

// One-time setup. Initialises USB CDC + SCPI parser. Call after
// rulos_hal_init() and before scheduler_run().
//
// `on_usb_tx_complete` is forwarded to the SCPI library and called from
// the USB CDC TX-complete callback so timestamper.c can drive its
// ping-pong streaming buffers off the same USB connection.
void timestamper_scpi_init(void (*on_usb_tx_complete)(void));
