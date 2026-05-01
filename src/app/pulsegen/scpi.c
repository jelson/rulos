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
 *   OUTPut[n]:STATe ON|OFF|1|0
 *   SOURce[n]:PULSe:PERiod  <seconds>
 *   SOURce[n]:PULSe:WIDTh   <seconds>
 *   SOURce[n]:PULSe:COUNt   <integer>
 *   SOURce[n]:BURSt:PERiod  <seconds>
 */

#include "scpi.h"

#include <stdbool.h>
#include <stdint.h>

#include "periph/scpi/scpi.h"
#include "pulsegen.h"

// NULL = success; non-NULL = pulsegen-supplied error string.
static void apply_setter_result(const char *err) {
  if (err) scpi_set_error(err);
  else scpi_clear_error();
}

static bool dispatch_line(const char *line) {
  // OUTPut[n]:STATe ON|OFF
  {
    const char *p = scpi_match_kw(line, "OUTPUT", "OUTP");
    if (p) {
      int ch;
      p = scpi_parse_channel_suffix(p, NUM_CHANNELS, &ch);
      if (*p == ':') {
        p++;
        const char *q = scpi_match_kw(p, "STATE", "STAT");
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

  // SOURce[n]:{PULSe:{PER,WIDT,COUN} | BURSt:PER} <val>
  {
    const char *p = scpi_match_kw(line, "SOURCE", "SOUR");
    if (p) {
      int ch;
      p = scpi_parse_channel_suffix(p, NUM_CHANNELS, &ch);
      if (*p == ':') {
        p++;

        const char *pulse = scpi_match_kw(p, "PULSE", "PULS");
        if (pulse && *pulse == ':') {
          pulse++;
          const char *q;
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
          if ((q = scpi_match_kw(pulse, "COUNT", "COUN")) && *q == ' ') {
            uint32_t v;
            if (!scpi_parse_uint(q + 1, &v)) {
              scpi_set_error("-104,\"Bad number\"");
              return true;
            }
            apply_setter_result(pulsegen_set_count(ch, v));
            return true;
          }
        }

        const char *burst = scpi_match_kw(p, "BURST", "BURS");
        if (burst && *burst == ':') {
          burst++;
          const char *q;
          if ((q = scpi_match_kw(burst, "PERIOD", "PER")) && *q == ' ') {
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

  return false;
}

void pulsegen_scpi_init(void) {
  const scpi_config_t cfg = {
      .idn = pulsegen_idn,
      .on_reset = pulsegen_reset_all,
      .on_line = dispatch_line,
  };
  scpi_init(&cfg);
}
