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

#include "periph/pms5003/pms5003.h"

#include <stdint.h>
#include <stdlib.h>

#include "core/rulos.h"
#include "core/util.h"
#include "periph/uart/uart.h"

static uint16_t bytes_to_int16(char *buf, int index) {
  return buf[index] << 8 | buf[index + 1];
}

static void _parse(pms5003_t *pms) {
  while (true) {
    char *buf = CharQueue_ptr(&pms->inq.q);
    int len = CharQueue_length(&pms->inq.q);

    // If we don't have at least 2 bytes for sync, stop
    if (len < 2) {
      return;
    }

    // Check sync bytes. On sync failure, delete one byte and try again
    if (buf[0] != 0x42 || buf[1] != 0x4D) {
      LOG("pms5003: out of sync, deleting one byte");
      CharQueue_pop_n(&pms->inq.q, NULL, 1);
      continue;
    }

    // If we don't have at least 4 bytes for sync and len, stop
    if (len < 4) {
      return;
    }

    // Parse frame len and ensure it's what we expect
    uint16_t frame_len = bytes_to_int16(buf, 2);
    if (frame_len != 28) {
      LOG("pms5003: invalid frame length");
      CharQueue_pop_n(&pms->inq.q, NULL, 4);
      continue;
    }

    // If the entire frame hasn't arrived yet, stop and wait
    if (len < 32) {
      return;
    }

    // compute checksum
    uint32_t computed = 0;
    for (int i = 0; i < 30; i++) {
      computed += buf[i];
    }
    uint16_t expected = bytes_to_int16(buf, 30);
    if (computed != expected) {
      LOG("pms5003: checksum mismatch");
      CharQueue_pop_n(&pms->inq.q, NULL, 32);
      continue;
    }

    pms5003_data_t d = {
        .pm10_standard = bytes_to_int16(buf, 4),
        .pm25_standard = bytes_to_int16(buf, 6),
        .pm100_standard = bytes_to_int16(buf, 8),
        .pm10_env = bytes_to_int16(buf, 10),
        .pm25_env = bytes_to_int16(buf, 12),
        .pm100_env = bytes_to_int16(buf, 14),
        .particles_03um = bytes_to_int16(buf, 16),
        .particles_05um = bytes_to_int16(buf, 18),
        .particles_10um = bytes_to_int16(buf, 20),
        .particles_25um = bytes_to_int16(buf, 22),
        .particles_50um = bytes_to_int16(buf, 24),
        .particles_100um = bytes_to_int16(buf, 26),
    };

    // upcall!
    pms->cb(&d, pms->user_data);
    CharQueue_pop_n(&pms->inq.q, NULL, 32);
  }
}

// called at task time
static void _rx_cb(UartState_t *u, void *user_data, char *buf, size_t len) {
  pms5003_t *pms = (pms5003_t *)user_data;
  if (!CharQueue_append_n(&pms->inq.q, buf, len)) {
    LOG("buffer overflow!");
  }
  _parse(pms);
}

void pms5003_init(pms5003_t *pms, uint8_t uart_id, pms5003_cb_t cb,
                  void *user_data) {
  assert(cb != NULL);
  memset(pms, 0, sizeof(*pms));
  CharQueue_init(&pms->inq.q, sizeof(pms->inq));
  CharQueue_append_n(&pms->inq.q, "hello", 5);
  pms->cb = cb;
  pms->user_data = user_data;
  uart_init(&pms->uart, uart_id, 9600);
  uart_start_rx(&pms->uart, _rx_cb, pms);
}
