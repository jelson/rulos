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
#include "event.h"
#include "seqmacro.h"

extern void syncdebug(uint8_t spaces, char f, uint16_t line);
//#define SYNCDEBUG()	syncdebug(0, 'R', __LINE__)
#define SYNCDEBUG()	{}
extern void audioled_set(r_bool red, r_bool yellow);

void _as_fill(Activation *act);
SEQDECL(ASStreamCard, as_stream_card, 0_init_sdc);
SEQDECL(ASStreamCard, as_stream_card, 1_loop);
SEQDECL(ASStreamCard, as_stream_card, 2_start_tx);
SEQDECL(ASStreamCard, as_stream_card, 3_more_frames);
SEQDECL(ASStreamCard, as_stream_card, 4_read_more);

void init_audio_streamer(AudioStreamer *as, uint8_t timer_id)
{
	as->fill_act.func = _as_fill;
	as->timer_id = timer_id;
	init_audio_out(&as->audio_out, as->timer_id, &as->fill_act);
	as->sdc_initialized = FALSE;
	sdc_init(&as->sdc);

	event_init(&as->ulawbuf_empty_evt, FALSE);
	event_init(&as->play_request_evt, TRUE);

	// allow the first read to proceed.
	event_signal(&as->ulawbuf_empty_evt);

	// start the ASStreamCard thread
	{
		as->assc.as = as;
		Activation *act = &as->assc.act;
		SEQNEXT(ASStreamCard, as_stream_card, 0_init_sdc);
		schedule_now(act);
	}
}


//////////////////////////////////////////////////////////////////////////////

#if 0
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
#endif

void _ad_decode_ulaw_buf(uint8_t *dst, uint8_t *src, uint16_t len, uint8_t mlvolume)
{
	uint8_t *end = src+len;
	for (; src<end; src++, dst++)
	{
#if 0
		int16_t vs = _ad_decode_ulaw(*src) - 128;
		vs = ((vs * volume) >> 8) + 128;
		*dst = vs & 0xff;
#endif
		//*dst = (*src) - 1;
		*dst = ((*src) + 128) >> mlvolume;
	}
}

void _as_fill(Activation *act)
{
	AudioStreamer *as = (AudioStreamer *) act;
	uint8_t *fill_ptr = as->audio_out.buffers[as->audio_out.fill_buffer];
//	SYNCDEBUG();
//	syncdebug(0, 'f', (int) as->audio_out.fill_buffer);
//	syncdebug(0, 'f', (int) fill_ptr);

	audioled_set(0, 1);
	// always clear buf in case we end up coming back around here
	// before ASStreamCard gets a chance to fill it. Want to play
	// silence, not stuttered audio.
	memset(fill_ptr, 127, AO_BUFLEN);

	if (!event_is_signaled(&as->ulawbuf_empty_evt))
	{
		// invariant: ulawbuf_ready == sdc_ready waiting for it;
		// not elsewise scheduled.
		audioled_set(1, 0);
		SYNCDEBUG();
		_ad_decode_ulaw_buf(fill_ptr, as->ulawbuf, AO_BUFLEN,
			as->is_music ? as->music_mlvolume : 0);
		event_signal(&as->ulawbuf_empty_evt);
	}
}

//////////////////////////////////////////////////////////////////////////////

SEQDEF(ASStreamCard, as_stream_card, 0_init_sdc, assc)
	AudioStreamer *as = assc->as;

	sdc_reset_card(&as->sdc, act);
	SEQNEXT(ASStreamCard, as_stream_card, 1_loop);	// implied
	return;
}

SEQDEF(ASStreamCard, as_stream_card, 1_loop, assc)
	AudioStreamer *as = assc->as;

	as->sdc_initialized = TRUE;
	if (as->block_address >= as->end_address)
	{
		// just finished an entire file. Tell caller, and wait
		// for a new request.
		if (as->done_act != NULL)
		{
			schedule_now(as->done_act);
		}

		SYNCDEBUG();
		event_wait(&as->play_request_evt, act);
		//SEQNEXT(ASStreamCard, as_stream_card, 1_loop);	// implied
		return;
	}

	// ready to start a read. Get an empty buffer first.
	SYNCDEBUG();
	event_wait(&as->ulawbuf_empty_evt, act);
	SEQNEXT(ASStreamCard, as_stream_card, 2_start_tx);
	return;
}

SEQDEF(ASStreamCard, as_stream_card, 2_start_tx, assc)
	AudioStreamer *as = assc->as;

	r_bool rc = sdc_start_transaction(&as->sdc,
		as->block_address,
		as->ulawbuf,
		AO_BUFLEN,
		act);
	if (!rc)
	{
		// dang, card was busy! the 'goto' didn't. try again in a millisecond.
		SYNCDEBUG();
		schedule_us(1000, act);
		return;
	}
	SEQNEXT(ASStreamCard, as_stream_card, 3_more_frames);
	return;
}

SEQDEF(ASStreamCard, as_stream_card, 3_more_frames, assc)
	AudioStreamer *as = assc->as;

	if (sdc_is_error(&as->sdc))
	{
		SYNCDEBUG();
		sdc_end_transaction(&as->sdc, act);
		SEQNEXT(ASStreamCard, as_stream_card, 0_init_sdc);
		return;
	}

	event_reset(&as->ulawbuf_empty_evt);
	as->sector_offset += AO_BUFLEN;
	if (as->sector_offset < SDBUFSIZE)
	{
		// Wait for more space to put data into.
		SYNCDEBUG();
		event_wait(&as->ulawbuf_empty_evt, act);
		SEQNEXT(ASStreamCard, as_stream_card, 4_read_more);
		return;
	}

	// done reading sector; loop back around to read more
	// (or a different sector, if redirected)
	as->block_address += SDBUFSIZE;
	as->sector_offset = 0;

	SYNCDEBUG();
	sdc_end_transaction(&as->sdc, act);
	SEQNEXT(ASStreamCard, as_stream_card, 1_loop);
	return;
}

SEQDEF(ASStreamCard, as_stream_card, 4_read_more, assc)
	AudioStreamer *as = assc->as;

	sdc_continue_transaction(&as->sdc,
		as->ulawbuf,
		AO_BUFLEN,
		act);
	SEQNEXT(ASStreamCard, as_stream_card, 3_more_frames);
	return;
}

//////////////////////////////////////////////////////////////////////////////

r_bool as_play(AudioStreamer *as, uint32_t block_address, uint16_t block_offset, uint32_t end_address, r_bool is_music, Activation *done_act)
{
	SYNCDEBUG();
	as->block_address = block_address;
	as->sector_offset = 0;
		// TODO reimplement preroll skippage
		// Just assigning this wouldn't help; it would cause us to
		// skip the END of the first block.
		// Maybe the best strategy is to end early rather than
		// start late.
	as->end_address = end_address;
	as->is_music = is_music;
	as->done_act = done_act;
		// TODO we just lose the previous callback in this case.
		// hope that's okay.
	event_signal(&as->play_request_evt);
	return TRUE;
}

void as_set_music_volume(AudioStreamer *as, uint8_t music_mlvolume)
{
	as->music_mlvolume = music_mlvolume;
}

void as_stop_streaming(AudioStreamer *as)
{
	as->end_address = 0;
}

SDCard *as_borrow_sdc(AudioStreamer *as)
{
	return as->sdc_initialized ? &as->sdc : NULL;
}
