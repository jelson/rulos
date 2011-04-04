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

#ifndef _spiflash_h
#define _spiflash_h

#include "rocket.h"
#include "hal.h"
#include "ring_buffer.h"

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
r_bool spiflash_next_buffer_ready(SPIFlash *spif);
void spiflash_fill_buffer(SPIFlash *spif, SPIBuffer *spib);

#endif // _spiflash_h
