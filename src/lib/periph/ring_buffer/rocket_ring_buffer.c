/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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

#include "periph/ring_buffer/rocket_ring_buffer.h"
#include "core/rulos.h"

void init_ring_buffer(RingBuffer *rb, uint16_t allocsize) {
  rb->capacity = allocsize - sizeof(RingBuffer);
  rb->insert = 0;
  rb->remove = 0;
}

// invariant: rb->buf[rb->insert] is an empty cell.
// (hence the capacity-1 property.)

uint8_t ring_insert_avail(RingBuffer *rb) {
  return rb->capacity - 1 - ring_remove_avail(rb);
}

void ring_insert(RingBuffer *rb, uint8_t data) {
  rb->buf[rb->insert] = data;
  rb->insert += 1;
  if (rb->insert >= rb->capacity) {
    rb->insert = 0;
  }
  assert(rb->insert != rb->remove);  // invariant fail: buffer full
}

uint8_t ring_remove_avail(RingBuffer *rb) {
  int16_t result = rb->insert - rb->remove;
  if (result < 0) {
    result += rb->capacity;
  }
  return result;
}

uint8_t ring_remove(RingBuffer *rb) {
  assert(rb->remove != rb->insert);  // buffer empty
  uint8_t result = rb->buf[rb->remove];
  rb->remove += 1;
  if (rb->remove >= rb->capacity) {
    rb->remove = 0;
  }
  return result;
}
