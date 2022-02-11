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

#include "core/rulos.h"
#include "periph/7seg_panel/7seg_panel.h"
#include "periph/uart/linereader.h"
#include "periph/uart/uart.h"

/***********************************************************************/
/***********************************************************************/

typedef struct {
  BoardBuffer bbuf_k;
  BoardBuffer bbuf_u;
  UartState_t uart;
  LineReader_t linereader;
} KeyTestActivation_t;

void add_char_to_bbuf(BoardBuffer *bbuf, char c, UartState_t *uart) {
  memmove(bbuf->buffer, bbuf->buffer + 1, NUM_DIGITS - 1);
  bbuf->buffer[NUM_DIGITS - 1] = ascii_to_bitmap(c);
  board_buffer_draw(bbuf);

  char out[100];
  memset(out, 'x', sizeof(out));
  strcpy(out, "boing! ");
  out[7] = c;
  out[8] = '\n';
  out[9] = '\0';
  LOG(out);
}

static void uart_line_received(UartState_t *uart, void *data, char *line) {
  KeyTestActivation_t *kta = (KeyTestActivation_t *)data;

  LOG("got uart line: '%s'", line);
  while (*line) {
    add_char_to_bbuf(&kta->bbuf_u, *line, &kta->uart);
    line++;
  }
}

static void update(KeyTestActivation_t *kta) {
  schedule_us(50000, (ActivationFuncPtr)update, kta);

  uint8_t c;

  while ((c = hal_read_keybuf()) != 0) {
    add_char_to_bbuf(&kta->bbuf_k, c, &kta->uart);
  }
}

#define TEST_STR "hello world from the uart\n"

typedef struct {
  BoardBuffer *bbuf;
  UartState_t *uart;
  int count;
} Metronome;

void tock(Metronome *m) {
  m->count = (m->count + 1) & 7;
  m->bbuf->buffer[0] = ascii_to_bitmap('0' + m->count);
  board_buffer_draw(m->bbuf);
}

void tick(Metronome *m) {
  LOG("tick");
  schedule_us(500000, (ActivationFuncPtr)tock, m);
  schedule_us(1000000, (ActivationFuncPtr)tick, m);
}

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER1);
  hal_init_keypad();  // requires clock to be initted.
  board_buffer_module_init();
  hal_init_rocketpanel();

  KeyTestActivation_t kta;
  uart_init(&kta.uart, /*uart_id=*/0, 38400);
  log_bind_uart(&kta.uart);
  linereader_init(&kta.linereader, &kta.uart, uart_line_received, &kta);
  LOG(TEST_STR);

  board_buffer_init(&kta.bbuf_k);
  board_buffer_push(&kta.bbuf_k, 0);

  board_buffer_init(&kta.bbuf_u);
  board_buffer_push(&kta.bbuf_u, 1);
  schedule_us(1, (ActivationFuncPtr)update, &kta);

  add_char_to_bbuf(&kta.bbuf_k, 'I', &kta.uart);
  add_char_to_bbuf(&kta.bbuf_k, 'n', &kta.uart);
  add_char_to_bbuf(&kta.bbuf_k, 'i', &kta.uart);
  add_char_to_bbuf(&kta.bbuf_k, 't', &kta.uart);

  Metronome m;
  m.uart = &kta.uart;
  m.bbuf = &kta.bbuf_k;
  schedule_us(1000000, (ActivationFuncPtr)tick, &m);

  scheduler_run();
  return 0;
}
