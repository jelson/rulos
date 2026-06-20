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
 */

/*
 * SCPI front-end for pulsegen. The generic SCPI library
 * (src/lib/periph/scpi) handles USB CDC, line buffering, and the IEEE
 * 488.2 mandatory commands; this file just dispatches the pulsegen-
 * specific commands and translates them into pulsegen_* setters.
 *
 * Supported app-specific commands (long and short forms):
 *   MODE ASYNC|SYNC                          MODE?
 *   OUTPut[n]:STATe ON|OFF|1|0
 *   SOURce[n]:PULSe:PERiod  <seconds>
 *   SOURce[n]:PULSe:WIDTh   <seconds>
 *   SOURce[n]:PULSe:DELay   <seconds>
 *   SOURce[n]:BURSt:STATe ON|OFF|1|0         SOURce[n]:BURSt:STATe?
 *   SOURce[n]:BURSt:NCYCles <count>          SOURce[n]:BURSt:NCYCles?
 *   SOURce[n]:BURSt:INTernal:PERiod <sec>    SOURce[n]:BURSt:INTernal:PERiod?
 */

#include "scpi.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "periph/scpi/scpi.h"
#include "pulsegen.h"

// NULL = success; non-NULL = pulsegen-supplied error string.
static void apply_setter_result(const char* err) {
  if (err) {
    scpi_set_error(err);
  } else {
    scpi_clear_error();
  }
}

static void print_uint(uint32_t v) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)v);
  scpi_print(buf);
}

// Print picoseconds as decimal seconds with a full 12-digit fraction (e.g.
// "1.000000000000"). The fraction is emitted as 3+9 digit halves so only
// 32-bit printf conversions are needed.
static void print_seconds_ps(uint64_t ps) {
  char buf[28];
  const uint64_t frac = ps % 1000000000000ULL;
  snprintf(buf, sizeof(buf), "%lu.%03lu%09lu", (unsigned long)(ps / 1000000000000ULL),
           (unsigned long)(frac / 1000000000ULL), (unsigned long)(frac % 1000000000ULL));
  scpi_print(buf);
}

static bool dispatch_line(const char* line) {
  // MODE ASYNC|SYNC  and  MODE?
  {
    const char* p = scpi_match_kw(line, "MODE", "MODE");
    if (p) {
      if (*p == '?') {
        scpi_print(pulsegen_mode_str());
        return true;
      }
      if (*p == ' ') {
        p++;
        if (scpi_match_kw(p, "SYNC", "SYNC")) {
          apply_setter_result(pulsegen_set_mode(true));
          return true;
        }
        if (scpi_match_kw(p, "ASYNC", "ASYNC")) {
          apply_setter_result(pulsegen_set_mode(false));
          return true;
        }
        scpi_set_error("-104,\"Expected ASYNC or SYNC\"");
        return true;
      }
    }
  }

  // OUTPut[n]:STATe ON|OFF
  {
    const char* p = scpi_match_kw(line, "OUTPUT", "OUTP");
    if (p) {
      int ch;
      p = scpi_parse_channel_suffix(p, NUM_CHANNELS, &ch);
      if (*p == ':') {
        p++;
        const char* q = scpi_match_kw(p, "STATE", "STAT");
        if (q && *q == ' ') {
          bool on;
          if (!scpi_parse_bool(q + 1, &on)) {
            scpi_set_error("-104,\"Bad boolean\"");
            return true;
          }
          apply_setter_result(pulsegen_set_state(ch, on));
          return true;
        }
      }
    }
  }

  // SOURce[n]:{PULSe | BURSt} subtrees
  {
    const char* p = scpi_match_kw(line, "SOURCE", "SOUR");
    if (p) {
      int ch;
      p = scpi_parse_channel_suffix(p, NUM_CHANNELS, &ch);
      if (*p == ':') {
        p++;

        const char* pulse = scpi_match_kw(p, "PULSE", "PULS");
        if (pulse && *pulse == ':') {
          pulse++;
          const char* q;
          if ((q = scpi_match_kw(pulse, "PERIOD", "PER")) && *q == ' ') {
            uint64_t v;
            if (!scpi_parse_seconds_ps(q + 1, &v)) {
              scpi_set_error("-104,\"Bad number\"");
              return true;
            }
            apply_setter_result(pulsegen_set_period_ps(ch, v));
            return true;
          }
          if ((q = scpi_match_kw(pulse, "WIDTH", "WIDT")) && *q == ' ') {
            uint64_t v;
            if (!scpi_parse_seconds_ps(q + 1, &v)) {
              scpi_set_error("-104,\"Bad number\"");
              return true;
            }
            apply_setter_result(pulsegen_set_width_ps(ch, v));
            return true;
          }
          if ((q = scpi_match_kw(pulse, "DELAY", "DEL")) && *q == ' ') {
            uint64_t v;
            if (!scpi_parse_seconds_ps(q + 1, &v)) {
              scpi_set_error("-104,\"Bad number\"");
              return true;
            }
            apply_setter_result(pulsegen_set_delay_ps(ch, v));
            return true;
          }
        }

        const char* burst = scpi_match_kw(p, "BURST", "BURS");
        if (burst && *burst == ':') {
          burst++;
          const char* q;
          if ((q = scpi_match_kw(burst, "STATE", "STAT"))) {
            if (*q == '?') {
              scpi_print(pulsegen_get_burst_state(ch) ? "ON" : "OFF");
              return true;
            }
            if (*q == ' ') {
              bool on;
              if (!scpi_parse_bool(q + 1, &on)) {
                scpi_set_error("-104,\"Bad boolean\"");
                return true;
              }
              apply_setter_result(pulsegen_set_burst_state(ch, on));
              return true;
            }
          }
          if ((q = scpi_match_kw(burst, "NCYCLES", "NCYC"))) {
            if (*q == '?') {
              print_uint(pulsegen_get_burst_ncyc(ch));
              return true;
            }
            if (*q == ' ') {
              uint32_t v;
              if (!scpi_parse_uint(q + 1, &v)) {
                scpi_set_error("-104,\"Bad number\"");
                return true;
              }
              apply_setter_result(pulsegen_set_burst_ncyc(ch, v));
              return true;
            }
          }
          const char* internal = scpi_match_kw(burst, "INTERNAL", "INT");
          if (internal && *internal == ':' && (q = scpi_match_kw(internal + 1, "PERIOD", "PER"))) {
            if (*q == '?') {
              print_seconds_ps(pulsegen_get_burst_period_ps(ch));
              return true;
            }
            if (*q == ' ') {
              uint64_t v;
              if (!scpi_parse_seconds_ps(q + 1, &v)) {
                scpi_set_error("-104,\"Bad number\"");
                return true;
              }
              apply_setter_result(pulsegen_set_burst_period_ps(ch, v));
              return true;
            }
          }
        }
      }
    }
  }

  return false;
}

void pulsegen_scpi_init(void) {
  const scpi_config_t cfg = {
      .version = PULSEGEN_FW_VERSION,
      .on_reset = pulsegen_reset_all,
      .on_line = dispatch_line,
  };
  scpi_init(&cfg);
}
