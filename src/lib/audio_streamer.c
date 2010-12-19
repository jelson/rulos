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
//#define SYNCDEBUG()	syncdebug(0, 'R', __LINE__)
#define SYNCDEBUG()	{}

void _as_fill(Activation *act);
void _as_initialize_complete(Activation *act);
void _as_start_streaming(Activation *act);

void init_audio_streamer(AudioStreamer *as, uint8_t timer_id)
{
	as->fill_act.func = _as_fill;
	as->timer_id = timer_id;
	as->streaming = FALSE;
	init_audio_out(&as->audio_out, as->timer_id, &as->fill_act);
	sdc_init(&as->sdc);
	as->initialize_complete.act.func = _as_initialize_complete;
	as->initialize_complete.as = as;
	as->start_streaming.act.func = _as_start_streaming;
	as->start_streaming.as = as;

	sdc_initialize(&as->sdc, &as->initialize_complete.act);
}

void _as_initialize_complete(Activation *act)
{
	AudioStreamer *as = ((InitializeCompleteAct *) act)->as;
	if (!as->sdc.complete)
	{
		// try again
		SYNCDEBUG();
		sdc_initialize(&as->sdc, &as->initialize_complete.act);
	}
}

// based on sample code at:
// http://www.speech.cs.cmu.edu/comp.speech/Section2/Q2.7.html
static inline int16_t _ad_ulaw2linear(uint8_t ulawbyte)
{
  static int32_t exp_lut[8] = {0,132,396,924,1980,4092,8316,16764};
  int8_t sign;
  uint8_t exponent;
  int32_t mantissa;
  int32_t sample;

  ulawbyte = ~ulawbyte;
  sign = (ulawbyte & 0x80);
  exponent = (ulawbyte >> 4) & 0x07;
  mantissa = ulawbyte & 0x0F;
  sample = exp_lut[exponent] + (mantissa << (exponent + 3));
  if (sign != 0) { sample = -sample; }

  return sample;
}

static inline uint8_t _ad_pcm16s_to_pcm8u(int16_t s)
{
	return (s>>8)+127;
}

static inline uint8_t _ad_decode_ulaw(uint8_t ulaw)
{
	return _ad_pcm16s_to_pcm8u(_ad_ulaw2linear(ulaw));
}

void _ad_decode_ulaw_buf(uint8_t *dst, uint8_t *src, uint8_t len)
{
	uint8_t *end = src+len;
	for (; src<end; src++, dst++)
	{
		*dst = _ad_decode_ulaw(*src);
	}
}

void _as_fill(Activation *act)
{
	extern void audioled_set(r_bool red, r_bool yellow);
	AudioStreamer *as = (AudioStreamer *) act;
	uint8_t *fill_ptr = as->audio_out.buffer+as->audio_out.fill_index;
	if (as->streaming)
	{
		SYNCDEBUG();
		audioled_set(1, 0);
		//memcpy(fill_ptr, &as->sdbuffer[as->block_offset], AO_HALFBUFLEN);
		uint8_t *nextbuf = &as->sdc.blockbuffer[as->block_offset];
		_ad_decode_ulaw_buf(fill_ptr, nextbuf, AO_HALFBUFLEN);
		void syncdebug32(uint8_t spaces, char f, uint32_t line);
		//syncdebug(2, 'p', as->block_offset);
		as->block_offset += AO_HALFBUFLEN;
		// NB here we assume SD block size is a multiple of AO_HALFBUFLEN,
		// so that SD block requests stay aligned.
		if (as->block_offset >= as->sdc.blocksize)
		{
			audioled_set(1, 1);
			// sdbuffer empty; start refill
			//syncdebug(1, 'd', (as->block_address)>>16);
			//syncdebug(1, 'd', (as->block_address)&0xffff);
			//syncdebug32(2, 'f', as->block_address);
			sdc_read(&as->sdc, as->block_address, NULL);
			as->block_offset = 0;
			as->block_address+=SDBUFSIZE;
			if (as->block_address >= as->end_address)
			{
				as->streaming = FALSE;
				if (as->done_act != NULL)
				{
					schedule_now(as->done_act);
				}
			}
		}
	}
	else
	{
		audioled_set(0, 1);
		memset(fill_ptr, 127, AO_HALFBUFLEN);
	}
}

void _as_start_streaming(Activation *act)
{
	SYNCDEBUG();
	StartStreamingAct *ssa = (StartStreamingAct *) act;
	ssa->as->streaming = TRUE;
}

r_bool as_play(AudioStreamer *as, uint32_t block_address, uint16_t block_offset, uint32_t end_address, Activation *done_act)
{
	// what happens if we're already streaming?
	// sdc->busy is true, so sdc_read does nothing, so we return,
	// so we never start trying to read sdc buffer until it's safe.

	// must set up suitable initial conditions to begin stream:
	// need first block already in memory and preroll pointer pointing
	// inside it before we expose it to _fill callback.
	r_bool ready = sdc_read(&as->sdc, block_address, &as->start_streaming.act);
	if (!ready) { return FALSE; }

	SYNCDEBUG();
	as->block_address = block_address+SDBUFSIZE;
	as->block_offset = block_offset;	// skip preroll bytes
	as->end_address = end_address;
	as->done_act = done_act;
	return TRUE;
}

void as_stop_streaming(AudioStreamer *as)
{
	as->streaming = FALSE;
}
