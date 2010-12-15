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

