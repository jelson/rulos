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

#include "periph/sdcard/sdcard.h"

#define R_SYNCDEBUG() syncdebug(0, 'S', __LINE__)
//#define SYNCDEBUG()	R_SYNCDEBUG()
#define SYNCDEBUG() \
  {}
extern void syncdebug(uint8_t spaces, char f, uint16_t line);

void fill_value(uint8_t *dst, uint32_t src);

void sdc_reset_card_cmd0(SDCard *sdc);
void sdc_reset_card_cmd1(SDCard *sdc);
void sdc_reset_card_cmd16(SDCard *sdc);
void sdc_reset_card_fini(SDCard *sdc);

//////////////////////////////////////////////////////////////////////////////

void _bunchaclocks_handler(HALSPIHandler *h, uint8_t data) {
  BunchaClocks *bc = (BunchaClocks *)h;
  if (bc->numclocks_bytes == 0) {
    schedule_us(10000, bc->done_rec.func, bc->done_rec.data);
  } else {
    bc->numclocks_bytes--;
    hal_spi_send(0xff);
  }
}

void bunchaclocks_start(BunchaClocks *bc, int numclocks_bytes,
                        ActivationFuncPtr done_func, void *done_data) {
  SYNCDEBUG();
  bc->handler.func = _bunchaclocks_handler;
  bc->numclocks_bytes = numclocks_bytes - 1;
  bc->done_rec.func = done_func;
  bc->done_rec.data = done_data;
  hal_spi_set_handler(&bc->handler);

  hal_spi_send(0xff);
}

//////////////////////////////////////////////////////////////////////////////

void sdc_init(SDCard *sdc) {
  sdc->transaction_open = FALSE;
  spi_init(&sdc->spi);
}

//////////////////////////////////////////////////////////////////////////////
// Reset card transaction
//////////////////////////////////////////////////////////////////////////////

void _sdc_issue_spic_cmd(SDCard *sdc, uint8_t *cmd, uint8_t cmdlen,
                         uint8_t cmd_expect_code, ActivationFuncPtr done_func) {
  SYNCDEBUG();
  sdc->spic.cmd = cmd;
  sdc->spic.cmdlen = cmdlen;
  sdc->spic.cmd_expect_code = cmd_expect_code;
  sdc->spic.reply_buffer = NULL;
  sdc->spic.reply_buflen = 0;
  sdc->spic.done_rec.func = done_func;
  sdc->spic.done_rec.data = sdc;
  spi_start(&sdc->spi, &sdc->spic);
}

#define SPI_CMD(x) ((x) | 0x40)
#define SPI_CMD0_CRC (0x95)
#define SPI_DUMMY_CRC (0x01)
uint8_t _spicmd0[6] = {SPI_CMD(0), 0, 0, 0, 0, SPI_CMD0_CRC};
uint8_t _spicmd1[6] = {SPI_CMD(1), 0, 0, 0, 0, SPI_DUMMY_CRC};
uint8_t _spicmd16[6] = {SPI_CMD(16), 0, 0, 0x20, 0x00, SPI_DUMMY_CRC};

void sdc_reset_card(SDCard *sdc, ActivationFuncPtr done_func, void *done_data) {
  SYNCDEBUG();
  sdc->transaction_open = TRUE;

  sdc->blocksize = 512;
  sdc->done_rec.func = done_func;
  sdc->done_rec.data = done_data;
  sdc->error = TRUE;

  hal_spi_set_fast(FALSE);
  bunchaclocks_start(&sdc->bunchaClocks, 16 + 16,
                     (ActivationFuncPtr)sdc_reset_card_cmd0, sdc);
}

#define SDCRESET_END(errorval)                          \
  SYNCDEBUG();                                          \
  sdc->error = errorval;                                \
  sdc->transaction_open = FALSE;                        \
  schedule_now(sdc->done_rec.func, sdc->done_rec.data); \
  return;

#define SDC_CHECK_SPI()          \
  SYNCDEBUG();                   \
  if (sdc->spic.error == TRUE) { \
    syncdebug(1, 'f', 0);        \
    SDCRESET_END(TRUE);          \
  }

void sdc_reset_card_cmd0(SDCard *sdc) {
  // No CHECK_SPI() because we haven't issued any yet;
  // error=TRUE right now just in case SPI doesn't set it.
  SYNCDEBUG();
  _sdc_issue_spic_cmd(sdc, _spicmd0, 6, 1,
                      (ActivationFuncPtr)sdc_reset_card_cmd1);
}

void sdc_reset_card_cmd1(SDCard *sdc) {
  SDC_CHECK_SPI();
  _sdc_issue_spic_cmd(sdc, _spicmd1, 6, 1,
                      (ActivationFuncPtr)sdc_reset_card_cmd16);
}

void sdc_reset_card_cmd16(SDCard *sdc) {
  SDC_CHECK_SPI();
  fill_value(&_spicmd16[1], sdc->blocksize);
  _sdc_issue_spic_cmd(sdc, _spicmd16, 6, 0,
                      (ActivationFuncPtr)sdc_reset_card_fini);
}

void sdc_reset_card_fini(SDCard *sdc) {
  SDC_CHECK_SPI();
  hal_spi_set_fast(TRUE);
  SDCRESET_END(FALSE);
}

//////////////////////////////////////////////////////////////////////////////
// Read transaction
//////////////////////////////////////////////////////////////////////////////

void fill_value(uint8_t *dst, uint32_t src) {
  dst[0] = (src >> (3 * 8)) & 0xff;
  dst[1] = (src >> (2 * 8)) & 0xff;
  dst[2] = (src >> (1 * 8)) & 0xff;
  dst[3] = (src >> (0 * 8)) & 0xff;
}

void sdcard_readmore(SDCard *sdc);

r_bool sdc_start_transaction(SDCard *sdc, uint32_t offset, uint8_t *buffer,
                             uint16_t buflen, ActivationFuncPtr done_func,
                             void *done_data) {
  if (sdc->transaction_open) {
    SYNCDEBUG();
    return FALSE;
  }
  SYNCDEBUG();
  sdc->transaction_open = TRUE;
  sdc->error = FALSE;

  uint8_t *read_cmdseq = sdc->read_cmdseq;
  read_cmdseq[0] = SPI_CMD(17);
  fill_value(&read_cmdseq[1], offset);
  read_cmdseq[5] = SPI_DUMMY_CRC;

  sdc->spic.cmd = read_cmdseq;
  sdc->spic.cmdlen = sizeof(sdc->read_cmdseq);
  sdc->spic.cmd_expect_code = 0;
  sdc->spic.done_rec.func = (ActivationFuncPtr)sdcard_readmore;
  sdc->spic.done_rec.data = sdc;

  sdc->spic.reply_buffer = buffer;
  sdc->spic.reply_buflen = buflen;
  sdc->done_rec.func = done_func;
  sdc->done_rec.data = done_data;

  spi_start(&sdc->spi, &sdc->spic);
  return TRUE;
}

void sdcard_readmore(SDCard *sdc) {
  if (sdc->spic.error) {
    SYNCDEBUG();
    sdc->error = TRUE;
  }
  sdc->done_rec.func(sdc->done_rec.data);
}

r_bool sdc_is_error(SDCard *sdc) { return sdc->error; }

void sdc_continue_transaction(SDCard *sdc, uint8_t *buffer, uint16_t buflen,
                              ActivationFuncPtr done_func, void *done_data) {
  // record the "return address" passed in here
  sdc->done_rec.func = done_func;
  sdc->done_rec.data = done_data;

  // tell spic how to return to me
  sdc->spic.done_rec.func = (ActivationFuncPtr)sdcard_readmore;
  sdc->spic.done_rec.data = sdc;

  // ...and ask it to pick up reading where it left off
  spi_resume_transfer(&sdc->spi, buffer, buflen);
}

void sdc_end_transaction(SDCard *sdc, ActivationFuncPtr done_func,
                         void *done_data) {
  // could clock out CRC bytes, but why?
  // finish spi on "separate thread".
  sdc->spic.done_rec.func = NULL;
  sdc->spic.done_rec.data = NULL;
  spi_finish(&sdc->spi);

  sdc->transaction_open = FALSE;
  if (done_func != NULL) {
    schedule_now(done_func, done_data);
  }
  SYNCDEBUG();
}
