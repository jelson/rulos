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

static void _lines_complete(void *data) {
  LineReader_t *l = (LineReader_t *)data;
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  char *s = CharQueue_ptr(&l->rx_queue.q);
  int len = CharQueue_length(&l->rx_queue.q);
  hal_end_atomic(old_interrupts);

  char *curr_string_start = s;
  int i = 0;
  while (true) {
    // Scan past any EOLs at the start of the string. These might be blank
    // lines, or the \n after an \r if we have DOS (CRLF) line endings.
    while (i < len && iseol(s[i])) {
      i++;
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
      l->cb(l->user_data, curr_string_start);
    } else {
      // No EOL found. stop.
      break;
    }
  }

  // LOG("consumed %d", i);
  old_interrupts = hal_start_atomic();
  CharQueue_pop_n(&l->rx_queue.q, NULL, i);
  l->cb_pending = false;
  hal_end_atomic(old_interrupts);
}

// warning, called at interrupt time!
static void _char_received(UartState_t *s, void *user_data, char c) {
  LineReader_t *l = (LineReader_t *)user_data;
  CharQueue_append(&l->rx_queue.q, c);
  if (iseol(c) && !l->cb_pending) {
    l->cb_pending = true;
    schedule_now(_lines_complete, l);
  }
}

void linereader_init(LineReader_t *l, UartState_t *uart, linereader_cb cb,
                     void *user_data) {
  assert(cb != NULL);
  memset(l, 0, sizeof(*l));
  l->cb = cb;
  l->user_data = user_data;
  CharQueue_init(&l->rx_queue.q, sizeof(l->rx_queue));
  uart_start_rx(uart, _char_received, l);
}
