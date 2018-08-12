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

#include "rulos.h"
#include "spi.h"

// Memory is too scarce to have separate callers with their own buffers;
// so we allocate a single block buffer, and share it with all callers.
#define SDBUFSIZE	512
#define SDCRCSIZE	2

typedef struct
{
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

r_bool sdc_start_transaction(
	SDCard *sdc,
	uint32_t offset,
	uint8_t *buffer,
	uint16_t buflen,
	ActivationFuncPtr done_func,
	void *done_data);
	// FALSE if sdc was busy.

r_bool sdc_is_error(SDCard *sdc);
	// only valid during a transaction.

void sdc_continue_transaction(
	SDCard *sdc,
	uint8_t *buffer,
	uint16_t buflen,
	ActivationFuncPtr done_func,
	void *done_data);

void sdc_end_transaction(SDCard *sdc, ActivationFuncPtr done_func, void *done_data);

