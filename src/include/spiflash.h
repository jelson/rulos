#ifndef _spiflash_h
#define _spiflash_h

#include "rocket.h"
#include "hal.h"

typedef enum {
	spist_read_send_addr,
	spist_read_send_dummy,
	spist_read_bytes,
} SPIFState;

typedef struct s_spiflash_buffer {
	uint32_t addr;
	uint8_t *buf;
	uint32_t size;
} SPIBuffer;

typedef struct s_spiflash {
	HALSPIFunc func;
	Activation *complete;
		// scheduled to indicate the current spif request has completed.
	SPIFState state;
	uint8_t addr[3];
	uint8_t addrptr;
	uint32_t offset;
	SPIBuffer *done_buf;
	SPIBuffer *active_buf;
	SPIBuffer *next_buf;

	// when we don't have an outstanding request, we busy-loop filling this
	// buffer. Makes it easier to reason about asynchrony, because we never
	// have to stop or start the fetching process; we're always doing the
	// same fill-buffer/next-buffer process, just sometimes to an empty
	// buffer.
	uint8_t dummy_space[1];
	SPIBuffer dummy_buf;
} SPIFlash;

void init_spiflash(SPIFlash *spif, Activation *complete);
void spiflash_queue_buffer(SPIFlash *spif, SPIBuffer *buf);
SPIBuffer *spiflash_collect_buffer(SPIFlash *spif);

#endif // _spiflash_h
