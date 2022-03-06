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

#include "periph/uart/linereader.h"

#include <string.h>

#include "core/rulos.h"
#include "periph/uart/uart.h"

static bool iseol(char c) {
  return (c == '\n' || c == '\r');
}

static void _parse_lines(LineReader_t *l) {
  char *s = CharQueue_ptr(&l->line_queue.q);
  int len = CharQueue_length(&l->line_queue.q);

  char *curr_string_start = s;
  int i = 0;
  int consumed = 0;
  while (true) {
    // Scan past any EOLs at the start of the string. These might be blank
    // lines, or the \n after an \r if we have DOS (CRLF) line endings.
    while (i < len && iseol(s[i])) {
      i++;
      consumed = i;
    }

    curr_string_start = &s[i];

    // If we've reached end-of-string, stop
    if (i == len) {
      break;
    }

    // Scan forward to find the next EOL
    while (i < len && !iseol(s[i])) {
      i++;
    }

    // If an EOL was found, change it to \0 and produce an upcall
    if (i < len) {
      s[i] = '\0';
      i++;
      consumed = i;
      l->cb(l->uart, l->user_data, curr_string_start);
    } else {
      // No EOL found. stop.
      break;
    }
  }

  CharQueue_pop_n(&l->line_queue.q, NULL, consumed);
  // int remaining = CharQueue_length(&l->line_queue.q);
  // LOG("%d consumed, %d remaining", consumed, remaining);
}

// Called at task time
static void _buf_received(UartState_t *s, void *user_data, char *buf,
                          size_t len) {
  LineReader_t *l = (LineReader_t *)user_data;
  while (len > 0) {
    size_t free_space = CharQueue_free_space(&l->line_queue.q);
    if (free_space == 0) {
      l->overflows++;
      LOG("linereader overflow!");
      CharQueue_pop_n(&l->line_queue.q, NULL, LINEREADER_MAX_LINE_LEN);
      return;
    }
    size_t write_size = r_min(free_space, len);
    CharQueue_append_n(&l->line_queue.q, (char *)buf, write_size);
    buf += write_size;
    len -= write_size;
    _parse_lines(l);
  }
}

void linereader_init(LineReader_t *l, UartState_t *uart, linereader_cb cb,
                     void *user_data) {
  assert(cb != NULL);
  memset(l, 0, sizeof(*l));
  l->uart = uart;
  l->cb = cb;
  l->user_data = user_data;
  CharQueue_init(&l->line_queue.q, sizeof(l->line_queue));
  uart_start_rx(uart, _buf_received, l);
}
