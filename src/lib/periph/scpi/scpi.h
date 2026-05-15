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
 * Generic SCPI front-end for RULOS apps that expose a USB CDC control
 * channel. The library owns the USB CDC port, the linereader that splits
 * incoming bytes into lines, the IEEE 488.2 mandatory commands (*IDN?,
 * *RST, *CLS, SYSTem:ERRor?), and the latched-error state. Each app
 * supplies a single dispatch callback that handles its own command set,
 * plus a few parser helpers for keyword matching and number parsing.
 *
 * The line-handling loop is:
 *
 *   1. Strip leading whitespace; ignore empty lines.
 *   2. Try the IEEE 488.2 mandatory commands. If matched, handle and stop.
 *   3. Call the app's dispatch_line callback. If it returns true, stop.
 *   4. Otherwise, latch a "Command error" so SYST:ERR? reveals it.
 *
 * The dispatch callback is responsible for emitting query responses with
 * scpi_print() and reporting setter failures with scpi_set_error().
 */

#include <stdbool.h>
#include <stdint.h>

#include "periph/usb_cdc/usb_cdc.h"

typedef struct {
  // Identity returned for *IDN?. Required.
  const char *idn;

  // Called when *RST is received. May be NULL.
  void (*on_reset)(void);

  // Called for any line that doesn't match an IEEE 488.2 mandatory command.
  // The line is null-terminated, has had leading whitespace stripped, and
  // has no trailing newline. Return true if the line was recognised
  // (whether it succeeded or generated a setter error via scpi_set_error);
  // return false to let the library latch a "Command error".
  bool (*on_line)(const char *line);

  // Optional: called from the USB CDC TX-complete callback after the SCPI
  // library updates its bookkeeping. Apps that stream bulk data through
  // the same USB CDC connection (e.g. a continuous timestamp stream) use
  // this to drive their own ping-pong buffers.
  void (*on_usb_tx_complete)(void);
} scpi_config_t;

// Initialise USB CDC + linereader and start dispatching incoming lines.
// Call after rulos_hal_init().
void scpi_init(const scpi_config_t *config);

// Send a single line to the host. A trailing '\n' is appended. No-op if
// USB is not enumerated or a previous transmission is still in flight, so
// callers should only emit lines they are willing to drop.
void scpi_print(const char *line);

// Replace the latched error string returned by the next SYST:ERR? query.
// The msg argument is copied; SCPI convention is `<num>,"<message>"`.
void scpi_set_error(const char *msg);

// Reset the latched error to `0,"No error"`.
void scpi_clear_error(void);

// True iff the host has opened the USB CDC port (DTR asserted).
bool scpi_usb_ready(void);

// Underlying USB CDC connection. Apps that need to stream bulk data
// through the same port (e.g. usbd_cdc_write) use this to share the
// connection with the SCPI parser. Trust LTO to inline this away: with
// -flto -O2 the body is just `return &<static_global>` and the compiler
// folds the call at every call site.
usbd_cdc_state_t *scpi_usb_cdc_handle(void);

// One-shot: returns true iff any RX or TX has happened since the last
// call. Use to drive a USB-activity LED.
bool scpi_consume_recent_activity(void);

// ---- Parser helpers --------------------------------------------------------

// Case-insensitive keyword match. Returns the position in s after the
// matched keyword (long or short form), or NULL if neither matched.
const char *scpi_match_kw(const char *s, const char *full,
                          const char *short_);

// If s starts with a digit in 0..(max_channel-1), set *ch to that digit
// and return s advanced past it. Otherwise set *ch to 0 and return s
// unchanged. Used for the optional 0-based channel suffix on commands
// like INPut3:.
const char *scpi_parse_channel_suffix(const char *s, int max_channel,
                                      int *ch);

// Parse decimal seconds (with optional fraction and optional `e`/`E`
// exponent) into picoseconds. Returns false on syntax error.
bool scpi_parse_seconds_ps(const char *s, uint64_t *out_ps);

// Parse a non-negative decimal integer. Returns false on syntax error.
bool scpi_parse_uint(const char *s, uint32_t *out);

// Parse a SCPI boolean (`ON`, `OFF`, `1`, `0`, case-insensitive).
bool scpi_parse_bool(const char *s, bool *out);
