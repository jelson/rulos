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
	HALSPIFunc func;
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
