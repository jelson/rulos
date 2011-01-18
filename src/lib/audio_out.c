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

#include "rocket.h"
#include "board_defs.h"
#include "hal.h"

#include "audio_out.h"

extern void syncdebug(uint8_t spaces, char f, uint16_t line);
#define SYNCDEBUG()	syncdebug(0, 'O', __LINE__)

void _ao_handler(void *data);

void init_audio_out(AudioOut *ao, uint8_t timer_id, Activation *fill_act)
{
	memset(ao->buffers, 127, sizeof(ao->buffers));
	int i;
	for (i=0; i<AO_BUFLEN; i++)
	{
		ao->buffers[0][i] = i*2;
		ao->buffers[1][AO_BUFLEN-i] = i*2;
	}
	ao->sample_index = 0;
	ao->fill_buffer = 0;
	ao->play_buffer = 0;
	ao->fill_act = fill_act;
	hal_audio_init();
#define SAMPLE_RATE	15000
	hal_start_clock_us(1000000/SAMPLE_RATE, &_ao_handler, ao, timer_id);
	SYNCDEBUG();
}

void _ao_handler(void *data)
{
	static uint8_t val = 0;
	val = 0x10+val;
	hal_audio_fire_latch();
	AudioOut *ao = (AudioOut *) data;
	uint8_t sample = ao->buffers[ao->play_buffer][ao->sample_index];
	hal_audio_shift_sample(sample);
	ao->sample_index = (ao->sample_index + 1) & AO_BUFMASK;
	if ((ao->sample_index & AO_BUFMASK)==0)
	{
		ao->fill_buffer = ao->play_buffer;
		ao->play_buffer += 1;
		if (ao->play_buffer >= AO_NUMBUFS)
		{
			ao->play_buffer = 0;
		}
		schedule_now(ao->fill_act);
	}
}
