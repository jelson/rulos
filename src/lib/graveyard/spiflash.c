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

#error THIS IS DEAD CODE.
// It was written intending to talk to a 4MB DIP flash chip;
// we ended up never even soldering it together, in favor of
// GB-range SD cards.

#include "graveyard/spiflash.h"

void _spif_start(SPIFlash *spif);
void _spiflash_receive(HALSPIHandler *hdlr, uint8_t byte);

void init_spiflash(SPIFlash *spif) {
  spif->handler.func = _spiflash_receive;
  hal_init_spi();

  spif->zero_buf.start_addr = 0;
  spif->zero_buf.count = 0;  // keeps us from trying to write into rb
  spif->zero_buf.rb = NULL;

  spif->spibuf[0] = &spif->zero_buf;
  spif->spibuf[1] = &spif->zero_buf;
  spif->cur_buf_index = 0;

  rulos_irq_state_t old_interrupts = hal_start_atomic();
  _spif_start(spif);
  hal_end_atomic(old_interrupts);
}

r_bool spiflash_next_buffer_ready(SPIFlash *spif) {
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  SPIBuffer *spib = spif->spibuf[1 - spif->cur_buf_index];
  hal_end_atomic(old_interrupts);
  return spib == &spif->zero_buf;
}

void spiflash_fill_buffer(SPIFlash *spif, SPIBuffer *spib) {
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  LOG("spiflash_fill_buffer got spib %08x => %d; rb %08x", (unsigned int)spib,
      1 - spif->cur_buf_index, (unsigned)spib->rb);
  spif->spibuf[1 - spif->cur_buf_index] = spib;

  LOG("spiflash_fill_buffer spib %8x spib->count %d spib->rb size %d",
      (int)spib, spib->count, ring_insert_avail(spib->rb));
  hal_end_atomic(old_interrupts);
}

// ATOMIC
void _spif_start(SPIFlash *spif) {
  // old buf is now idle
  spif->spibuf[spif->cur_buf_index] = &spif->zero_buf;

  // other buf is now current
  uint8_t cbi = 1 - spif->cur_buf_index;
  spif->cur_buf_index = cbi;

  // ATMEL is little-endian; flash wants 3 big-endian bytes
  uint32_t start_addr = spif->spibuf[cbi]->start_addr;
  spif->addr[0] = start_addr >> 16;
  spif->addr[1] = start_addr >> 8;
  spif->addr[2] = start_addr >> 0;
  spif->addrptr = 0;

  LOG("spib %08x (%d) spiflash initiates at %2x %2x %2x, count %d",
      (unsigned int)spif->spibuf[cbi], cbi, spif->addr[0], spif->addr[1],
      spif->addr[2], spif->spibuf[spif->cur_buf_index]->count);

  spif->state = spist_read_send_addr;
  hal_spi_open();
  hal_spi_send(SPIFLASH_CMD_READ_DATA);
}

// INTERRUPT CONTEXT
void _spiflash_receive(SPIFlash *spif, uint8_t byte) {
  switch (spif->state) {
    case spist_read_send_addr: {
      uint8_t sendbyte = spif->addr[spif->addrptr++];
      if (spif->addrptr == 3) {
        spif->state = spist_read_send_dummy;
      }
      hal_spi_send(sendbyte);
      break;
    }
    case spist_read_send_dummy:
      spif->state = spist_read_bytes;
      goto _spiflash_read_more;
    case spist_read_bytes: {
      SPIBuffer *spib = spif->spibuf[spif->cur_buf_index];
      ring_insert(spib->rb, byte);
      spib->count -= 1;
      LOG("_spiflash_receive spib %8x spib->count %d spib->rb size %d",
          (int)spib, spib->count, ring_insert_avail(spib->rb));
      goto _spiflash_read_more;
    }
  }
  return;

_spiflash_read_more : {
  SPIBuffer *spib = spif->spibuf[spif->cur_buf_index];
  if (spib->count > 0 && ring_insert_avail(spib->rb) > 0) {
    hal_spi_send(0x00);
  } else {
    spif->spibuf[spif->cur_buf_index]->rb = NULL;
    hal_spi_close();
    _spif_start(spif);
  }
}
}
