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

#include "periph/7seg_panel/board_buffer.h"

#include <stdio.h>

#include "core/rulos.h"
#include "periph/7seg_panel/remote_bbuf.h"

#define BBUF_UNMAPPED_INDEX (0xff)

BoardBuffer *foreground[NUM_TOTAL_BOARDS];

#if BBDEBUG
void dump(const char *prefix) {
  int b;
  LOG("-----");
  for (b = 0; b < NUM_TOTAL_BOARDS; b++) {
    LOG("%s b%2d: ", prefix, b);
    BoardBuffer *buf = foreground[b];
    while (buf != NULL) {
      LOG("%s ", buf->label);
      buf = buf->next;
    }
    LOG("<end>");
  }
}
#endif  // BBDEBUG

RemoteBBufSend *g_remote_bbuf_send;

void board_buffer_module_init() {
  int i;
  for (i = 0; i < NUM_TOTAL_BOARDS; i++) {
    foreground[i] = NULL;
  }
  g_remote_bbuf_send = NULL;
}

void install_remote_bbuf_send(RemoteBBufSend *rbs) {
  g_remote_bbuf_send = rbs;
}

void board_buffer_init(BoardBuffer *buf DBG_BBUF_LABEL_DECL) {
  int i;
  for (i = 0; i < NUM_DIGITS; i++) {
    buf->buffer[i] = 0;
  }
  buf->next = NULL;
  buf->board_index = BBUF_UNMAPPED_INDEX;
  buf->alpha = 0xff;
  assert(sizeof(buf->alpha) * 8 >= NUM_DIGITS);
#if BBDEBUG
  buf->label = label;
#endif
}

void _board_buffer_compute_mask(BoardBuffer *buf, uint8_t redraw) {
  uint8_t left = 0xff;
  while (buf != NULL) {
    buf->mask = left & buf->alpha;
    left = left & (~buf->alpha);
    if (redraw) {
      board_buffer_draw(buf);
    }
    buf = buf->next;
  }
}

void clear_board(uint8_t index) {
  SSBitmap empty[NUM_DIGITS];
  for (int i = 0; i < NUM_DIGITS; i++) {
    empty[i] = 0;
  }
  display_controller_program_board(index, empty);
}

void board_buffer_pop(BoardBuffer *buf) {
  if (board_buffer_is_foreground(buf)) {
    if (buf->next == NULL) {
      // will the last buffer to leave, please turn out the lights?
#if NUM_LOCAL_BOARDS > 0
      clear_board(buf->board_index);
#endif
      foreground[buf->board_index] = NULL;
    } else {
      foreground[buf->board_index] = buf->next;
    }
  } else {
    // TODO if restoring remote display code: this path not implemented.
    assert(buf->board_index < NUM_TOTAL_BOARDS);
    BoardBuffer *prevbuf = foreground[buf->board_index];
    uint8_t loopcheck = 0;
    while (prevbuf->next != buf) {
      prevbuf = prevbuf->next;
      assert(prevbuf != NULL);
      loopcheck += 1;
      assert(loopcheck < 100);
    }
    prevbuf->next = buf->next;
  }

  _board_buffer_compute_mask(buf->next,
                             TRUE);  // fyi okay even when buf->next==NULL
  buf->next = NULL;
  buf->board_index = BBUF_UNMAPPED_INDEX;

#if BBDEBUG
  dump("  after pop");
#endif
}

void board_buffer_push(BoardBuffer *buf, int board) {
  assert(board < NUM_TOTAL_BOARDS);
#if BBDEBUG
  dump("before push");
#endif  // BBDEBUG
  buf->board_index = board;
  buf->next = foreground[board];
  foreground[board] = buf;
  _board_buffer_compute_mask(buf, FALSE);  // no redraw because...
  board_buffer_draw(buf);  // ...this line will overwrite existing
#if BBDEBUG
  dump(" after push");
#endif  // BBDEBUG
}

void board_buffer_set_alpha(BoardBuffer *buf, uint8_t alpha) {
  buf->alpha = alpha;

  if (board_buffer_is_stacked(buf)) {
    _board_buffer_compute_mask(foreground[buf->board_index], TRUE);
  }
}

void board_buffer_draw(BoardBuffer *buf) {
  uint8_t board_index = buf->board_index;
#if BBDEBUG
  LOG("bb_draw(3, buf %016" PRIxPTR " %s, mask %x)", (uintptr_t)buf, buf->label,
      buf->mask);
#endif  // BBDEBUG

// draw locally, if we can.
#if NUM_LOCAL_BOARDS > 0
  if (0 <= board_index && board_index < NUM_LOCAL_BOARDS) {
    board_buffer_paint(buf->buffer, board_index, buf->mask);
  }
#endif  // NUM_LOCAL_BOARDS > 0
#if NUM_REMOTE_BOARDS > 0
  if (g_remote_bbuf_send != NULL && NUM_LOCAL_BOARDS <= board_index &&
      board_index < NUM_TOTAL_BOARDS) {
    // jonh hard-codes remote send ability, rather than getting all
    // objecty about it, because doing this well with polymorphism
    // really wants a dynamic memory allocator.
    send_remote_bbuf(g_remote_bbuf_send, buf->buffer,
                     board_index - NUM_LOCAL_BOARDS, buf->mask);
  }
#endif  // NUM_REMOTE_BOARDS > 0
}

void board_buffer_paint(SSBitmap *bm, uint8_t board_index, uint8_t mask) {
  uint8_t idx;
  for (idx = 0; idx < NUM_DIGITS; idx++, mask <<= 1) {
    if (mask & 0x80) {
      SSBitmap tmp = bm[idx];
      display_controller_program_cell(board_index, idx, tmp);
    }
  }
  display_controller_enter_sleep();
}

uint8_t board_buffer_is_foreground(BoardBuffer *buf) {
  return (buf->board_index < NUM_TOTAL_BOARDS &&
          foreground[buf->board_index] == buf);
}

bool board_buffer_is_stacked(BoardBuffer *buf) {
  return buf->board_index != BBUF_UNMAPPED_INDEX;
}
