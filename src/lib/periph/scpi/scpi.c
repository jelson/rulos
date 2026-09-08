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

#include "scpi.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "core/util.h"
#include "periph/uart/linereader.h"
#include "periph/usb_cdc/usb_cdc.h"

// Dispatch stops after each reply until this buffer is available again.
// The unconsumed USB OUT packet stays in the CDC buffer with RX paused,
// so arbitrarily large command bursts use bounded RAM and host backpressure.
#define SCPI_TX_BUFLEN 128

// *IDN? response buffer. The format "<vendor>,<model>,<serial>,<firmware>"
// is bounded by the lengths the app supplies in scpi_config_t plus the
// USB serial; 128 bytes leaves room for a typical vendor name, a product
// name, the prefix+UID serial, and a version/commit firmware tag.
#define SCPI_IDN_BUFLEN 128

static usbd_cdc_state_t scpi_usb_cdc;
static LineReader_t linereader;
static char tx_buf[SCPI_TX_BUFLEN];
static uint32_t tx_len;
static bool tx_in_flight;
static const uint8_t *rx_data;
static uint32_t rx_len;
static char idn_buf[SCPI_IDN_BUFLEN];
static volatile bool recent_activity = false;
static scpi_config_t cfg;

// Last error reported, returned by the next SYST:ERR?.
static char err_msg[80] = "0,\"No error\"";
static uint32_t err_generation;
static uint32_t reply_error_generation;
static bool reply_is_error;

void scpi_set_error(const char *msg) {
  strncpy(err_msg, msg, sizeof(err_msg) - 1);
  err_msg[sizeof(err_msg) - 1] = 0;
  err_generation++;
}

void scpi_clear_error(void) {
  strcpy(err_msg, "0,\"No error\"");
  err_generation++;
}

const char *scpi_match_kw(const char *s, const char *full, const char *short_) {
  size_t lf = strlen(full);
  size_t ls = strlen(short_);
  if (strncasecmp(s, full, lf) == 0) {
    return s + lf;
  }
  if (strncasecmp(s, short_, ls) == 0) {
    return s + ls;
  }
  return NULL;
}

const char *scpi_parse_channel_suffix(const char *s, int max_channel, int *ch) {
  if (max_channel >= 1 && *s >= '0' && *s < ('0' + max_channel)) {
    *ch = *s - '0';
    return s + 1;
  }
  *ch = 0;
  return s;
}

bool scpi_parse_seconds_ps(const char *s, uint64_t *out) {
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  if (*s == 0) {
    return false;
  }
  if (*s == '+') {
    s++;
  }
  if (*s == '-') {
    return false;
  }

  uint64_t mant = 0;
  int frac_digits = 0;
  bool seen_digit = false;
  while (*s >= '0' && *s <= '9') {
    if (mant > (UINT64_MAX - 9) / 10) {
      return false;
    }
    mant = mant * 10 + (*s - '0');
    seen_digit = true;
    s++;
  }
  if (*s == '.') {
    s++;
    while (*s >= '0' && *s <= '9') {
      // Once mant saturates, drop further fractional digits silently.
      if (mant <= (UINT64_MAX - 9) / 10) {
        mant = mant * 10 + (*s - '0');
        frac_digits++;
      }
      seen_digit = true;
      s++;
    }
  }
  if (!seen_digit) {
    return false;
  }

  int exp = 0;
  if (*s == 'e' || *s == 'E') {
    s++;
    bool exp_neg = false;
    if (*s == '+') {
      s++;
    } else if (*s == '-') {
      exp_neg = true;
      s++;
    }
    if (!(*s >= '0' && *s <= '9')) {
      return false;
    }
    while (*s >= '0' && *s <= '9') {
      exp = exp * 10 + (*s - '0');
      if (exp > 100) {
        return false;
      }
      s++;
    }
    if (exp_neg) {
      exp = -exp;
    }
  }
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  if (*s != 0) {
    return false;
  }

  // seconds -> picoseconds is *10^12; subtract the fractional digits already
  // absorbed into mant and add the explicit exponent.
  int net = 12 + exp - frac_digits;
  if (net >= 0) {
    for (int i = 0; i < net; i++) {
      if (mant > UINT64_MAX / 10) {
        return false;
      }
      mant *= 10;
    }
  } else {
    for (int i = 0; i < -net; i++) {
      mant /= 10;
    }
  }
  *out = mant;
  return true;
}

bool scpi_parse_uint(const char *s, uint32_t *out) {
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  if (*s == 0) {
    return false;
  }
  uint32_t v = 0;
  bool seen = false;
  while (*s >= '0' && *s <= '9') {
    if (v > (UINT32_MAX - 9) / 10) {
      return false;
    }
    v = v * 10 + (*s - '0');
    seen = true;
    s++;
  }
  if (!seen) {
    return false;
  }
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  if (*s != 0) {
    return false;
  }
  *out = v;
  return true;
}

bool scpi_parse_bool(const char *s, bool *out) {
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  if (strncasecmp(s, "ON", 2) == 0) {
    *out = true;
    return true;
  }
  if (strncasecmp(s, "OFF", 3) == 0) {
    *out = false;
    return true;
  }
  if (s[0] == '1') {
    *out = true;
    return true;
  }
  if (s[0] == '0') {
    *out = false;
    return true;
  }
  return false;
}

// ---- Standard-command dispatch ---------------------------------------------

// Try each IEEE 488.2 mandatory command. Return true if the line was
// matched and handled.
static bool try_standard(const char *line) {
  if (strncasecmp(line, "*IDN?", 5) == 0) {
    scpi_print(idn_buf);
    return true;
  }
  if (strncasecmp(line, "*RST", 4) == 0) {
    scpi_clear_error();
    if (cfg.on_reset) {
      cfg.on_reset();
    }
    return true;
  }
  if (strncasecmp(line, "*CLS", 4) == 0) {
    scpi_clear_error();
    return true;
  }

  const char *p = scpi_match_kw(line, "SYSTEM", "SYST");
  if (p && (*p == ':' || *p == ' ')) {
    if (*p == ':') {
      p++;
    }
    if (scpi_match_kw(p, "ERROR?", "ERR?")) {
      if (scpi_print(err_msg)) {
        reply_error_generation = err_generation;
        reply_is_error = true;
      }
      return true;
    }
  }
  return false;
}

static void handle_line(char *line) {
  while (*line == ' ' || *line == '\t') {
    line++;
  }
  if (*line == 0) {
    return;
  }

  if (try_standard(line)) {
    return;
  }
  if (cfg.on_line && cfg.on_line(line)) {
    return;
  }

  scpi_set_error("-100,\"Command error\"");
}

// ---- USB CDC plumbing ------------------------------------------------------

static void send_reply(void) {
  if (tx_len && !tx_in_flight && usbd_cdc_tx_ready(&scpi_usb_cdc)) {
    tx_in_flight = usbd_cdc_write(&scpi_usb_cdc, tx_buf, tx_len) == 0;
  }
}

bool scpi_print(const char *line) {
  if (tx_len) {
    return false;
  }
  size_t n = strlen(line);
  if (n + 1 > SCPI_TX_BUFLEN) {
    n = SCPI_TX_BUFLEN - 1;
  }
  memcpy(tx_buf, line, n);
  tx_buf[n++] = '\n';
  tx_len = n;
  send_reply();
  recent_activity = true;
  return true;
}

bool scpi_usb_ready(void) {
  return scpi_usb_cdc.usb_ready;
}

usbd_cdc_state_t *scpi_usb_cdc_handle(void) {
  return &scpi_usb_cdc;
}

bool scpi_consume_recent_activity(void) {
  if (recent_activity) {
    recent_activity = false;
    return true;
  }
  return false;
}

static void on_line(UartState_t *uart, void *user_data, char *line) {
  handle_line(line);
}

static void process_rx(void) {
  if (!scpi_usb_ready()) {
    return;
  }
  send_reply();
  while (rx_len && !tx_len) {
    // Feed at most one line: linereader callbacks are synchronous, so the
    // next command cannot overwrite a reply or clear an undelivered error.
    uint32_t n = 0;
    while (n < rx_len) {
      uint8_t c = rx_data[n++];
      if (c == '\r' || c == '\n') {
        break;
      }
    }
    linereader_feed(&linereader, (const char *)rx_data, n);
    rx_data += n;
    rx_len -= n;
  }
  if (!rx_len && !tx_len) {
    usbd_cdc_resume_rx(&scpi_usb_cdc);
  }
}

static void on_usb_rx(usbd_cdc_state_t *cdc, void *user_data, const uint8_t *data, uint32_t len) {
  recent_activity = true;
  if (!scpi_usb_ready()) {
    return;
  }
  usbd_cdc_pause_rx(cdc);
  rx_data = data;
  rx_len = len;
  process_rx();
}

static void on_usb_tx_complete(usbd_cdc_state_t *cdc, void *user_data) {
  recent_activity = true;
  bool was_reply = tx_in_flight;
  if (was_reply) {
    if (reply_is_error && reply_error_generation == err_generation) {
      scpi_clear_error();
    }
    reply_is_error = false;
    tx_in_flight = false;
    tx_len = 0;
  }

  // Alternate pending replies and application streaming transfers. A reply
  // waiting behind stream data wins the next turn; after a reply, the app
  // can start one stream transfer before the following command is handled.
  if (was_reply && cfg.on_usb_tx_complete) {
    cfg.on_usb_tx_complete();
  }
  process_rx();
  if (!was_reply && cfg.on_usb_tx_complete) {
    cfg.on_usb_tx_complete();
  }
}

static void on_usb_connect(usbd_cdc_state_t *cdc, void *user_data) {
  process_rx();
}

static void on_usb_disconnect(usbd_cdc_state_t *cdc, void *user_data) {
  rx_data = NULL;
  rx_len = 0;
  linereader_init_unbound(&linereader, NULL, on_line, NULL);
  // DTR deassertion does not cancel an in-flight transfer. Do not reuse its
  // storage until completion; a physical USB deinit does cancel it.
  // A new application transfer can start before this deferred callback, so
  // identify our buffer rather than treating every busy TX as our response.
  if (cdc->tx_buf_in_flight != tx_buf) {
    tx_in_flight = false;
  }
  if (!tx_in_flight) {
    tx_len = 0;
    reply_is_error = false;
  }
  usbd_cdc_resume_rx(cdc);
}

void scpi_init(const scpi_config_t *config) {
  cfg = *config;
  tx_len = 0;
  tx_in_flight = false;
  rx_data = NULL;
  rx_len = 0;
  reply_is_error = false;

  // Build the IEEE 488.2 *IDN? response once. The vendor and model fields are
  // the USB iManufacturer / iProduct strings (USBD_MANUFACTURER_STRING /
  // USBD_PRODUCT_STRING) -- one source of truth, so the IDN can't drift from
  // the USB enumeration name. The serial matches the USB iSerialNumber
  // descriptor (USBD_SERIAL_PREFIX + STM32 96-bit UID). The firmware field is
  // "<version>-<commit>" if the app supplied a version, else just "<commit>".
  char serial[USBD_SERIAL_BUFLEN];
  usbd_cdc_get_serial(serial);
  if (cfg.version) {
    snprintf(idn_buf, sizeof(idn_buf),
             USBD_MANUFACTURER_STRING "," USBD_PRODUCT_STRING ",%s,%s-" STRINGIFY(GIT_COMMIT),
             serial, cfg.version);
  } else {
    snprintf(idn_buf, sizeof(idn_buf),
             USBD_MANUFACTURER_STRING "," USBD_PRODUCT_STRING ",%s," STRINGIFY(GIT_COMMIT), serial);
  }

  linereader_init_unbound(&linereader, /*uart=*/NULL, on_line, NULL);

  scpi_usb_cdc = (usbd_cdc_state_t){
      .rx_cb = on_usb_rx,
      .tx_complete_cb = on_usb_tx_complete,
      .connect_cb = on_usb_connect,
      .disconnect_cb = on_usb_disconnect,
      .user_data = NULL,
  };
  usbd_cdc_init(&scpi_usb_cdc);
}
