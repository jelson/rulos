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
 * SCPI dispatcher for the timestamper. The generic SCPI library
 * (src/lib/periph/scpi) handles USB CDC, line buffering, and the IEEE
 * 488.2 mandatory commands; this file matches the timestamper-specific
 * commands and translates them into timestamper_* setter/getter calls.
 *
 * See scpi.h for the supported command list.
 */

#include "scpi.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "periph/scpi/scpi.h"
#include "timestamper.h"

static void send_uint(const char *prefix, uint32_t v) {
  char buf[32];
  if (prefix && *prefix) {
    snprintf(buf, sizeof(buf), "%s %lu", prefix, (unsigned long)v);
  } else {
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)v);
  }
  scpi_print(buf);
}

// INPut[n]:SLOPe {POS|POSITIVE|NEG|NEGATIVE|BOTH|EITH|EITHER}
// INPut[n]:SLOPe?
static bool handle_slope(int ch, const char *arg) {
  if (*arg == '?' && (arg[1] == 0 || arg[1] == ' ')) {
    switch (timestamper_get_slope(ch)) {
      case TIMESTAMPER_SLOPE_FALLING: scpi_print("NEG");  break;
      case TIMESTAMPER_SLOPE_BOTH:    scpi_print("BOTH"); break;
      case TIMESTAMPER_SLOPE_RISING:
      default:                        scpi_print("POS");  break;
    }
    return true;
  }
  if (*arg != ' ') return false;
  arg++;
  while (*arg == ' ' || *arg == '\t') arg++;
  if (scpi_match_kw(arg, "POSITIVE", "POS")) {
    timestamper_set_slope(ch, TIMESTAMPER_SLOPE_RISING);
    scpi_clear_error();
    return true;
  }
  if (scpi_match_kw(arg, "NEGATIVE", "NEG")) {
    timestamper_set_slope(ch, TIMESTAMPER_SLOPE_FALLING);
    scpi_clear_error();
    return true;
  }
  // Accept "BOTH" (most common) and "EITHer" (Keysight/SCPI-style alias).
  if (scpi_match_kw(arg, "BOTH", "BOTH") ||
      scpi_match_kw(arg, "EITHER", "EITH")) {
    timestamper_set_slope(ch, TIMESTAMPER_SLOPE_BOTH);
    scpi_clear_error();
    return true;
  }
  scpi_set_error("-104,\"Bad slope\"");
  return true;
}

// INPut[n]:DIVider <N> | INPut[n]:DIVider?
static bool handle_divider(int ch, const char *arg) {
  if (*arg == '?' && (arg[1] == 0 || arg[1] == ' ')) {
    send_uint(NULL, timestamper_get_divider(ch));
    return true;
  }
  if (*arg != ' ') return false;
  uint32_t n;
  if (!scpi_parse_uint(arg + 1, &n)) {
    scpi_set_error("-104,\"Bad number\"");
    return true;
  }
  if (n == 0) {
    scpi_set_error("-222,\"Divider must be >= 1\"");
    return true;
  }
  timestamper_set_divider(ch, n);
  scpi_clear_error();
  return true;
}

// FORMat[:DATA] TEXT|BINary | FORMat[:DATA]?
static bool handle_format(const char *arg) {
  if (*arg == '?' && (arg[1] == 0 || arg[1] == ' ')) {
    scpi_print(timestamper_get_format() == TIMESTAMPER_FORMAT_BINARY
               ? "BIN" : "TEXT");
    return true;
  }
  if (*arg != ' ') return false;
  arg++;
  while (*arg == ' ' || *arg == '\t') arg++;
  if (scpi_match_kw(arg, "TEXT", "TEXT")) {
    timestamper_set_format(TIMESTAMPER_FORMAT_TEXT);
    scpi_clear_error();
    return true;
  }
  if (scpi_match_kw(arg, "BINARY", "BIN")) {
    timestamper_set_format(TIMESTAMPER_FORMAT_BINARY);
    scpi_clear_error();
    return true;
  }
  scpi_set_error("-104,\"Bad format\"");
  return true;
}

// OUTPut:STATe ON|OFF | OUTPut:STATe?
static bool handle_output_state(const char *arg) {
  if (*arg == '?' && (arg[1] == 0 || arg[1] == ' ')) {
    scpi_print(timestamper_get_stream_enabled() ? "1" : "0");
    return true;
  }
  if (*arg != ' ') return false;
  bool on;
  if (!scpi_parse_bool(arg + 1, &on)) {
    scpi_set_error("-104,\"Bad boolean\"");
    return true;
  }
  timestamper_set_stream_enabled(on);
  scpi_clear_error();
  return true;
}

static bool dispatch_line(const char *line) {
  // OUTPut subsystem: STATe (stream gate) and CLEar (drop pending).
  {
    const char *p = scpi_match_kw(line, "OUTPUT", "OUTP");
    if (p && *p == ':') {
      p++;
      const char *q;
      if ((q = scpi_match_kw(p, "STATE", "STAT"))) {
        return handle_output_state(q);
      }
      // OUTPut:CLEar -- drop any pending timestamps in the device's
      // ring buffer and the in-flight USB TX half, then queue a
      // "# output discarded" marker so the host can synchronize.
      // Channel config (slope, divider, format, stream-enable) is
      // preserved.
      if ((q = scpi_match_kw(p, "CLEAR", "CLE")) && *q == 0) {
        timestamper_discard_pending();
        scpi_clear_error();
        return true;
      }
    }
  }

  // FORMat[:DATA] -- selects ASCII vs binary output stream.
  {
    const char *p = scpi_match_kw(line, "FORMAT", "FORM");
    if (p) {
      // Accept "FORM <arg>", "FORM?" and "FORM:DATA <arg>", "FORM:DATA?".
      if (*p == ':') {
        p++;
        const char *q = scpi_match_kw(p, "DATA", "DATA");
        if (q) return handle_format(q);
      } else {
        return handle_format(p);
      }
    }
  }

  // INPut[n]:{SLOPe|DIVider}
  const char *p = scpi_match_kw(line, "INPUT", "INP");
  if (!p) return false;

  int ch;
  p = scpi_parse_channel_suffix(p, NUM_CHANNELS, &ch);
  if (*p != ':') return false;
  p++;

  const char *q;
  if ((q = scpi_match_kw(p, "SLOPE", "SLOP"))) {
    return handle_slope(ch, q);
  }
  if ((q = scpi_match_kw(p, "DIVIDER", "DIV"))) {
    return handle_divider(ch, q);
  }
  return false;
}

void timestamper_scpi_init(void (*on_usb_tx_complete)(void)) {
  const scpi_config_t cfg = {
      .idn = timestamper_idn,
      .on_reset = timestamper_reset_all,
      .on_line = dispatch_line,
      .on_usb_tx_complete = on_usb_tx_complete,
  };
  scpi_init(&cfg);
}
