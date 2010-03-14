
#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>

#include "rocket.h"
#include "hardware.h"
#include "board_defs.h"
#include "hal.h"



// TODO arrange for hardware to call audio_emit_sample once every 125us
// (8kHz), either by speeding up the main clock (and only firing the clock
// handler every n ticks), or by using a separate CPU clock (TIMER0)

#define AUDIO_BUF_SIZE 32
static struct s_hardware_audio {
	RingBuffer *ring;
	uint8_t _storage[sizeof(RingBuffer)+1+AUDIO_BUF_SIZE];
} audio =
	{ NULL };

void audio_write_sample(uint8_t value)
{
	// TODO send this out to the DAC
}

void audio_emit_sample()
{
	if (audio.ring==NULL)
	{
		// not initialized.
		return;
	}

	// runs in interrupt context; atomic.
	uint8_t sample = 0;
	if (ring_remove_avail(audio.ring))
	{
		sample = ring_remove(audio.ring);
	}
	audio_write_sample(sample);
}

RingBuffer *hal_audio_init(uint16_t sample_period_us)
{
	assert(sample_period_us == 125);	// not that an assert in hardware.c helps...
	audio.ring = (RingBuffer*) audio._storage;
	init_ring_buffer(audio.ring, sizeof(audio._storage));
	return audio.ring;
}

//////////////////////////////////////////////////////////////////////////////
// SPI: ATMega...32P page 82, 169
// Flash: w25x16 page 17+

HALSPIIfc *g_spi;

ISR(SPI_STC_vect)
{
	uint8_t byte = SPDR;
	g_spi->func(g_spi, byte);	// read from SPDR?
}

#define DDR_SPI	DDRB
#define DD_MOSI	DDB3
#define DD_SCK	DDB5
#define DD_SS	DDB2
#define SPI_SS	GPIO_B2
// TODO these can't be right -- GPIO_B2 is allocated to BOARDSEL2
// in V11PCB.

void hal_init_spi(HALSPIIfc *spi)
{
	g_spi = spi;
	// TODO I'm blasting the port B DDR here; we may care
	// about correct values for PB[0167].
	DDR_SPI = (1<<DD_MOSI)|(1<<DD_SCK)|(1<<DD_SS);
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0);
}

void hal_spi_open()
{
	gpio_set_or_clr(SPI_SS, 0);
}

void hal_spi_send(uint8_t byte)
{
	SPDR = byte;
}

void hal_spi_close()
{
	gpio_set_or_clr(SPI_SS, 1);
}

