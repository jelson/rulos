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

#include "core/rulos.h"
#include "periph/spi/spi.h"

// Memory is too scarce to have separate callers with their own buffers;
// so we allocate a single block buffer, and share it with all callers.
#define SDBUFSIZE 512
#define SDCRCSIZE 2

typedef struct {
  HALSPIHandler handler;
  int numclocks_bytes;
  ActivationRecord done_rec;
} BunchaClocks;

typedef struct {
  uint16_t blocksize;
  SPI spi;
  SPICmd spic;
  BunchaClocks bunchaClocks;
  uint8_t read_cmdseq[6];
  r_bool transaction_open;
  r_bool error;
  ActivationRecord done_rec;
} SDCard;

void sdc_init(SDCard *sdc);

void sdc_reset_card(SDCard *sdc, ActivationFuncPtr done_func, void *done_data);

r_bool sdc_start_transaction(SDCard *sdc, uint32_t offset, uint8_t *buffer,
                             uint16_t buflen, ActivationFuncPtr done_func,
                             void *done_data);
// FALSE if sdc was busy.

r_bool sdc_is_error(SDCard *sdc);
// only valid during a transaction.

void sdc_continue_transaction(SDCard *sdc, uint8_t *buffer, uint16_t buflen,
                              ActivationFuncPtr done_func, void *done_data);

void sdc_end_transaction(SDCard *sdc, ActivationFuncPtr done_func,
                         void *done_data);
