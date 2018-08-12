/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#pragma once

#include "core/rulos.h"

typedef struct
{
	uint8_t *cmd;
	uint8_t cmdlen;
	uint8_t cmd_expect_code;
	uint8_t *reply_buffer;
	uint16_t reply_buflen;
	r_bool error;
	ActivationRecord done_rec;
} SPICmd;

struct s_SPI;

typedef void (_SPIResumeFunc)(struct s_SPI *spi);

typedef struct s_SPI
{
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
void spi_resume_transfer(SPI *spi, uint8_t *reply_buffer, uint16_t reply_buflen);
void spi_finish(SPI *spi);

