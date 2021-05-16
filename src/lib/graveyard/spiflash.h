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

#pragma once

#include "core/hal.h"
#include "periph/ring_buffer/rocket_ring_buffer.h"
#include "periph/rocket/rocket.h"

typedef enum {
  spist_read_send_addr,
  spist_read_send_dummy,
  spist_read_bytes,
} SPIFState;

typedef struct s_spiflash_buffer {
  uint32_t start_addr;
  uint32_t count;
  RingBuffer *rb;
} SPIBuffer;

typedef struct s_spiflash {
  HALSPIHandler handler;
  // one of those goofy superclass-interface-inheritance things, must-be-first.
  // scheduled to indicate the current spif request has completed.
  SPIFState state;
  uint8_t addr[3];
  uint8_t addrptr;

  SPIBuffer zero_buf;
  SPIBuffer *spibuf[2];
  uint8_t cur_buf_index;
} SPIFlash;

void init_spiflash(SPIFlash *spif);
bool spiflash_next_buffer_ready(SPIFlash *spif);
void spiflash_fill_buffer(SPIFlash *spif, SPIBuffer *spib);
