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

#pragma once

#include <inttypes.h>

typedef struct {
  uint8_t capacity;  // actually holds capacity-1 bytes;
                     // last byte is a sentinel
  uint8_t insert;
  uint8_t remove;
  uint8_t buf[1];  // dummy byte to "automatically" allocate extra byte
} RingBuffer;

#define DECLARE_RING_BUFFER(name, capacity)                     \
  union {                                                       \
    RingBuffer name;                                            \
    uint8_t _storage_##name[sizeof(RingBuffer) + capacity + 1]; \
  };
//	uint8_t[sizeof(RingBuffer)+capacity] storage_##name;
//	RingBuffer *name = (RingBuffer*) storage_##name;

void init_ring_buffer(RingBuffer *rb, uint16_t allocsize);
uint8_t ring_insert_avail(RingBuffer *rb);
void ring_insert(RingBuffer *rb, uint8_t data);
uint8_t ring_remove_avail(RingBuffer *rb);
uint8_t ring_remove(RingBuffer *rb);
