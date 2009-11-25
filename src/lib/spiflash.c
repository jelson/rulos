#include "spiflash.h"

void _spif_start(SPIFlash *spif);
void spiflash_receive(SPIFlash *spif, uint8_t byte);

void init_spiflash(SPIFlash *spif, Activation *complete)
{
	spif->func = (HALSPIFunc) spiflash_receive;
	spif->complete = complete;
	hal_init_spi((HALSPIIfc *) spif);

	spif->dummy_buf.addr = 0;
	spif->dummy_buf.buf = spif->dummy_space;
	spif->dummy_buf.size = 0;

	spif->done_buf = NULL;
	spif->active_buf = &spif->dummy_buf;
	spif->next_buf = &spif->dummy_buf;
	_spif_start(spif);
}

void spiflash_queue_buffer(SPIFlash *spif, SPIBuffer *buf)
{
	hal_start_atomic();
	assert(spif->next_buf == NULL);
	spif->next_buf = buf;
	hal_end_atomic();
}

SPIBuffer *spiflash_collect_buffer(SPIFlash *spif)
{
	SPIBuffer *result;
	hal_start_atomic();
	result = spif->done_buf;
	hal_end_atomic();
	return result;
}

void _spif_next_buffer(SPIFlash *spif)
{
	if (spif->active_buf != &spif->dummy_buf)
	{
		spif->done_buf = spif->active_buf;
		schedule_now(spif->complete);
	}
	else
	{
		spif->done_buf = NULL;
	}
	spif->active_buf = spif->next_buf;
	spif->next_buf = &spif->dummy_buf;

	// ATMEL is little-endian; flash wants 3 big-endian bytes
	spif->addr[0] = spif->active_buf->addr>>24;
	spif->addr[1] = spif->active_buf->addr>>16;
	spif->addr[2] = spif->active_buf->addr>> 8;
	spif->addrptr = 0;
	spif->offset = 0;
}

void _spif_start(SPIFlash *spif)
{
	_spif_next_buffer(spif);
	spif->state = spist_read_send_addr;
	hal_spi_open();
	hal_spi_send(SPIFLASH_CMD_READ_DATA);
}

void spiflash_receive(SPIFlash *spif, uint8_t byte)
{
	switch (spif->state)
	{
		case spist_read_send_addr:
			hal_spi_send(spif->addr[spif->addrptr++]);
			if (spif->addrptr == 3)
			{
				spif->state = spist_read_send_dummy;
			}
			break;
		case spist_read_send_dummy:
			hal_spi_send(0x00);
			spif->state = spist_read_bytes;
			break;
		case spist_read_bytes:
			spif->active_buf->buf[spif->offset++] = byte;
			if (spif->offset < spif->active_buf->size)
			{
				hal_spi_send(0x00);
			}
			else
			{
				hal_spi_close();
				_spif_start(spif);
			}
			break;
	}
}

