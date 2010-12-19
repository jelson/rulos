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

#include "audio_streamer.h"

extern void syncdebug(uint8_t spaces, char f, uint16_t line);
#define SYNCDEBUG()	syncdebug(0, 'R', __LINE__)

void _as_fill(Activation *act);
void _as_start_audio(Activation *act);

void init_audio_streamer(AudioStreamer *as, uint8_t timer_id)
{
	as->fill_act.func = _as_fill;
	as->block_address = 0;
	sdc_init(&as->sdc);
	as->start_audio_act.act.func = _as_start_audio;
	as->start_audio_act.as = as;
	as->timer_id = timer_id;
	sdc_initialize(&as->sdc, &as->start_audio_act.act);
}

void _as_start_audio(Activation *act)
{
	AudioStreamer *as = ((StartAudioAct *) act)->as;
	if (as->sdc.complete)
	{
		SYNCDEBUG();
		// let the fill requests start rolling in
		init_audio_out(&as->audio_out, as->timer_id, &as->fill_act);
	}
	else
	{
		// try again
		sdc_initialize(&as->sdc, &as->start_audio_act.act);
	}
}

void _as_fill(Activation *act)
{
	extern void audioled_set(r_bool red, r_bool yellow);
	audioled_set(1, 1);
	AudioStreamer *as = (AudioStreamer *) act;
	uint8_t *fill_ptr = as->audio_out.buffer+as->audio_out.fill_index;
	if (as->block_address == 0)
	{
		SYNCDEBUG();
		memset(fill_ptr, 0, AO_HALFBUFLEN);
	}
	else
	{
		SYNCDEBUG();
		memcpy(fill_ptr, &as->sdbuffer[as->block_offset], AO_HALFBUFLEN);
		as->block_offset += AO_HALFBUFLEN;
		if (as->block_offset >= as->sdc.blocksize)
		{
			// sdbuffer empty; start refill
			sdc_read(&as->sdc, as->block_address, as->sdbuffer, sizeof(as->sdbuffer), NULL);
			as->block_address+=sizeof(SDBUFSIZE);
			if (as->block_address >= as->end_address)
			{
				as->block_address = 0;
				if (as->done_act != NULL)
				{
					schedule_now(as->done_act);
				}
			}
		}
	}
}

void as_play(AudioStreamer *as, uint32_t block_address, uint16_t block_offset, uint32_t end_address, Activation *done_act)
{
	as->block_address = block_address;
	as->block_offset = block_offset;
	as->end_address = end_address;
	as->done_act = done_act;
}
