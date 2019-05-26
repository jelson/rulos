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

#include "periph/sdcard/sdcard.h"

#define R_SYNCDEBUG() syncdebug(0, 'I', __LINE__)
//#define SYNCDEBUG()	R_SYNCDEBUG()
#define SYNCDEBUG() \
  {}
extern void syncdebug(uint8_t spaces, char f, uint16_t line);

// TODO on logic analyzer:
// How slow are we going due to off-interrupt rescheduling?

// Why's there such a noticeably-long delay between last message
// and the PrintMsgAct firing?
// That one's easy: we're waiting for the HANDLER_DELAY of 1 clock
// (1ms) * 512 bytes to read = 512ms.

//////////////////////////////////////////////////////////////////////////////

#include "spi.h"
#include "hal_spi.h"

void _spi_spif_handler(HALSPIHandler *h, uint8_t data);

void spi_0_send_cmd(SPI *spi);
void spi_1_wait_cmd_ack(SPI *spi);
void spi_2_wait_reply(SPI *spi);
void spi_3_wait_buffer(SPI *spi);

void spi_init(SPI *spi) {
  SYNCDEBUG();
  spi->spic = NULL;
  hal_init_spi();
}

#define SPI_CMD_WAIT_LIMIT 500
#define SPI_REPLY_WAIT_LIMIT 500

void spi_start(SPI *spi, SPICmd *spic) {
  SYNCDEBUG();
  spic->error = FALSE;
  spi->spic = spic;

  spi->cmd_i = 0;
  spi->cmd_wait = SPI_CMD_WAIT_LIMIT;
  spi->reply_wait = SPI_REPLY_WAIT_LIMIT;

  hal_spi_select_slave(TRUE);
  spi->handler.func = _spi_spif_handler;
  hal_spi_set_handler(&spi->handler);

  spi_0_send_cmd(spi);
}

#define DBG_DATA_TRACE 0
#if DBG_DATA_TRACE
void _spi_debug_data(SPI *spi) {
  syncdebug(4, 'd', spi->data);
  syncdebug(4, 'h', ((int)spi->handler.func) << 1);
  (spi->resume)(spi);
}
#endif

void _spi_spif_handler(HALSPIHandler *h, uint8_t data) {
  SPI *spi = (SPI *)h;
  spi->data = data;
#if DBG_DATA_TRACE
  schedule_now((ActivationFuncPtr)_spi_debug_data, spi);
#else
  schedule_now((ActivationFuncPtr)spi->resume, spi);
#endif
}

void _spi_spif_fastread_handler(HALSPIHandler *h, uint8_t data) {
  SPI *spi = (SPI *)h;
  SPICmd *spic = spi->spic;

  *(spic->reply_buffer) = data;
  spic->reply_buffer += 1;
  spic->reply_buflen -= 1;
  if ((spic->reply_buflen) > 0) {
    // ready for next byte in fast handler
    hal_spi_send(0xff);
  } else {
    // nowhere to put next byte; go ask for more buffer
    _spi_spif_handler(h, data);
  }
}

#define READABLE(c) (((c) >= ' ' && (c) < 127) ? (c) : '_')

#define SPIFINISH(result)                                     \
  {                                                           \
    spic->error = result;                                     \
    hal_spi_select_slave(FALSE);                              \
    if (spic->done_rec.func != NULL) {                        \
      schedule_now(spic->done_rec.func, spic->done_rec.data); \
    }                                                         \
  }

void spi_0_send_cmd(SPI *spi) {
  SPICmd *spic = spi->spic;
  SYNCDEBUG();

  uint8_t out_val = spic->cmd[spi->cmd_i];
#if DBG_DATA_TRACE
  syncdebug(4, 'o', out_val);
  syncdebug(4, 'i', spi->cmd_i);
  syncdebug(4, 'l', spic->cmdlen);
#endif  // DBG_DATA_TRACE
  spi->cmd_i++;
  if (spi->cmd_i >= spic->cmdlen) {
    SYNCDEBUG();
    spi->resume = spi_1_wait_cmd_ack;
  } else {
    spi->resume = spi_0_send_cmd;
  }
  // Be careful to send spi message only after we've configured
  // resume, as it may get read out and stuffed into the clock
  // heap before we realize it.
  hal_spi_send(out_val);
}

void spi_1_wait_cmd_ack(SPI *spi) {
  SPICmd *spic = spi->spic;
  SYNCDEBUG();

#if DBG_DATA_TRACE
  syncdebug(5, 'd', spi->data);
#endif  // DBG_DATA_TRACE
  if (spi->data == spic->cmd_expect_code) {
    // arrange to process future incoming bytes in 2_wait_reply
    spi->resume = spi_2_wait_reply;

    // process a fake byte in 2_wait_reply, so we can decide
    // whether to stop here, happy, or request more (data) bytes
    spi->data = 0xff;
    spi_2_wait_reply(spi);

    // NB we don't drop/reopen CS line, per
    // SecureDigitalCard_1.9.doc page 31, page 3-6, sec 3.3.
    // recurse to start looking for the data block
  } else if (spi->cmd_wait == 0) {
    SYNCDEBUG();
    SPIFINISH(TRUE);
  } else if (spi->data & 0x80) {
    // SYNCDEBUG();
    hal_spi_send(0xff);
    spi->cmd_wait -= 1;
  } else {
    R_SYNCDEBUG();
    syncdebug(3, 'g', spi->data);
    syncdebug(3, 'e', spic->cmd_expect_code);
    // expect_code mismatch
    SPIFINISH(TRUE);
  }
}

void spi_2_wait_reply(SPI *spi) {
  SPICmd *spic = spi->spic;
  SYNCDEBUG();

  if (spic->reply_buflen == 0)  // not expecting any data
  {
    SYNCDEBUG();
    SPIFINISH(FALSE);
  } else if (spi->data == 0xfe)  // signals start of reply data
  {
    SYNCDEBUG();
    // assumes we have a buffer ready right now,
    // so this first byte has somewhere to go.
    spi->resume = spi_3_wait_buffer;
    spi->handler.func = _spi_spif_fastread_handler;
    hal_spi_send(0xff);
  } else if (spi->reply_wait == 0)  // timeout
  {
    SYNCDEBUG();
    SPIFINISH(TRUE);
  } else if (spi->data & 0x80)  // still waiting
  {
    SYNCDEBUG();
    // keep asking
    hal_spi_send(0xff);
    spi->reply_wait -= 1;
  } else  // invalid reply code
  {
    syncdebug(4, 'v', spi->data);
    SYNCDEBUG();
    SPIFINISH(TRUE);
  }
}

void spi_3_wait_buffer(SPI *spi) {
  SPICmd *spic = spi->spic;
  SYNCDEBUG();

  // prompt caller for more buffer space
  schedule_now(spic->done_rec.func, spic->done_rec.data);
}

void spi_resume_transfer(SPI *spi, uint8_t *reply_buffer,
                         uint16_t reply_buflen) {
  spi->spic->reply_buffer = reply_buffer;
  spi->spic->reply_buflen = reply_buflen;
  // poke at SPI to restart flow of bytes into fast handler
  hal_spi_send(0xff);
  // NB act still points at 3_wait_buffer, so when handler runs
  // out of space, we'll get dropped back into 3_wait_buffer
}

void spi_finish(SPI *spi) {
  SPICmd *spic = spi->spic;
  SYNCDEBUG();
  SPIFINISH(FALSE);
}
