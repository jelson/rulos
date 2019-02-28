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

#include "core/rulos.h"

typedef struct {
  uint8_t *cmd;
  uint8_t cmdlen;
  uint8_t cmd_expect_code;
  uint8_t *reply_buffer;
  uint16_t reply_buflen;
  r_bool error;
  ActivationRecord done_rec;
} SPICmd;

struct s_SPI;

typedef void(_SPIResumeFunc)(struct s_SPI *spi);

typedef struct s_SPI {
  HALSPIHandler handler;
  SPICmd *spic;
  uint8_t cmd_i;
  uint16_t cmd_wait;
  uint16_t reply_wait;

  // do most work out of interrupt handler (in an activation);
  // our hal_spi handler jumps into resume to continue.
  _SPIResumeFunc *resume;

  uint8_t data;
} SPI;

void spi_init(SPI *spi);
void spi_start(SPI *spi, SPICmd *spic);
void spi_resume_transfer(SPI *spi, uint8_t *reply_buffer,
                         uint16_t reply_buflen);
void spi_finish(SPI *spi);
