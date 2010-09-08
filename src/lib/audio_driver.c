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

#include "audio_driver.h"

#define AUDIO_UPDATE_INTERVAL 1000
#define SPI_READ_SIZE 10

void audio_update(AudioDriver *ad);
uint8_t _ad_decode_ulaw(uint8_t ulaw);
uint8_t _ad_pcm16s_to_pcm8u(int16_t s);

typedef struct {
	uint32_t start;
	uint32_t length;
} AudioClip;

#include "audio_index.ch"

void init_audio_stream(AudioStream *as)
{
	as->cur_token = sound_silence;
	as->cur_offset = 0;
	as->loop_token = sound_silence;

	as->ring = (RingBuffer*) as->_ring_buffer_storage;
	init_ring_buffer(as->ring, sizeof(as->_ring_buffer_storage));
}

void init_audio_driver(AudioDriver *ad)
{
	init_spiflash(&ad->spif);
	
	int i;
	for (i=0; i<NUM_STREAMS; i++)
	{
		init_audio_stream(&ad->stream[i]);
	}

	ad->spibspace[0].rb = NULL;
	ad->spibspace[1].rb = NULL;
	ad->next_spib = 0;
	ad->output_buffer = hal_audio_init(125);
	ad->func = (ActivationFunc) audio_update;

	schedule_us(1, (Activation*) ad);
}

void audio_stream_refill(AudioDriver *ad, AudioStream *as CONDSIMARG(uint8_t stream_index))
{
	uint8_t insert_avail = ring_insert_avail(as->ring);
	if (insert_avail == 0)
	{
		return;
	}

	AudioClip *ac = &audio_clips[as->cur_token];
	if (ac->length - as->cur_offset == 0)
	{
		LOGF((logfp, "refill; idx %d at %d\n", as->cur_token, as->cur_offset));
		// refill
		as->cur_token = as->loop_token;
		as->cur_offset = 0;
		// recompute
		ac = &audio_clips[as->cur_token];
	}
	uint16_t fill = min(min(
		SPI_READ_SIZE, ac->length - as->cur_offset), insert_avail);

	SPIBuffer *spib = &ad->spibspace[ad->next_spib];
	assert(spib->rb==NULL);	// debug to ensure we're not double-using spibspace.
	// (I don't think we can be, since there's only room for two such
	// pointers in spiflash, but I like to assert.)
	ad->next_spib = 1-ad->next_spib;

	spib->start_addr = ac->start + as->cur_offset;
	spib->count = fill;
	spib->rb = as->ring;

	LOGF((logfp, "stream %d requested %d; satisfied %d bytes from %d.%d (bytes at %d)\n",
		stream_index, insert_avail, fill, as->cur_token, as->cur_offset, spib->start_addr));

	as->cur_offset += fill;

	spiflash_fill_buffer(&ad->spif, spib);
}

void audio_update(AudioDriver *ad)
{
	schedule_us(AUDIO_UPDATE_INTERVAL, (Activation*) ad);
	uint8_t old_interrupts;

#if TEST_OUTPUT_ONLY
	old_interrupts = hal_start_atomic();
	uint8_t output_insert_avail = ring_insert_avail(ad->output_buffer);
	hal_end_atomic(old_interrupts);
	uint8_t c;
	for (c=0; c<output_insert_avail; c++)
	{
		ring_insert(ad->output_buffer, (ad->test_output_val & 0x8) ? 69 : 3);
		ad->test_output_val += 1;
	}

#else // TEST_OUTPUT_ONLY

	// jonh not very happy about blocking interrupts for collecting this stuff.
	old_interrupts = hal_start_atomic();
	uint8_t output_insert_avail = ring_insert_avail(ad->output_buffer);
	hal_end_atomic(old_interrupts);
	int sidx;
	int8_t most_desperate_stream_index = -1;
	uint8_t stream_remove_avail[NUM_STREAMS];
	for (sidx=0; sidx<NUM_STREAMS; sidx++)
	{
		old_interrupts = hal_start_atomic();
		stream_remove_avail[sidx] = ring_remove_avail(ad->stream[sidx].ring);
		hal_end_atomic(old_interrupts);

		if (most_desperate_stream_index==-1 ||
			(stream_remove_avail[sidx]
				< stream_remove_avail[most_desperate_stream_index]))
		{
			most_desperate_stream_index = sidx;
		}
	}
	assert(most_desperate_stream_index >= 0 && most_desperate_stream_index < NUM_STREAMS);
	
	// inserts and removes can be done non-atomically, because they
	// only touch vars that we only ever update. (They do assert on
	// the opposite value, but in a way that is protected by monotonicity.)

	// composite bytes into output stream until we're out of output space,
	// filling non-ready input streams with zeros.
	LOGF((logfp, "  output_insert_avail %d\n", output_insert_avail));
	while (output_insert_avail>0)
	{
		LOGF((logfp, "  output_insert_avail %d, ring sez %d\n", output_insert_avail, ring_insert_avail(ad->output_buffer)));
		uint8_t composite_sample = 6;
		for (sidx=0; sidx<NUM_STREAMS; sidx++)
		{
			if (stream_remove_avail[sidx] > 0)
			{
				stream_remove_avail[sidx]-=1;
				uint8_t ulaw_sample = ring_remove(ad->stream[sidx].ring);
				uint8_t pcm_sample = _ad_decode_ulaw(ulaw_sample);
				composite_sample += pcm_sample;
				//LOGF((logfp, "  stream %d supplies ulaw %2x -> %2x\n", sidx, ulaw_sample, pcm_sample));
			}
			else
			{
				//LOGF((logfp, "  stream %d empty; filling with 0\n", sidx));
			}
		}
		ring_insert(ad->output_buffer, composite_sample);
		LOGF((logfp, "  composite_sample %2x\n", composite_sample));
		output_insert_avail -= 1;
	}

	// pick an input stream to begin refilling.
	if (spiflash_next_buffer_ready(&ad->spif))
	{
		audio_stream_refill(ad, &ad->stream[most_desperate_stream_index] CONDSIMARG(most_desperate_stream_index));
	}
#endif // TEST_OUTPUT_ONLY
}

// based on sample code at:
// http://www.speech.cs.cmu.edu/comp.speech/Section2/Q2.7.html
int16_t _ad_ulaw2linear(uint8_t ulawbyte)
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

uint8_t _ad_pcm16s_to_pcm8u(int16_t s)
{
	return (s>>8)+127;
}

uint8_t _ad_decode_ulaw(uint8_t ulaw)
{
	return _ad_pcm16s_to_pcm8u(_ad_ulaw2linear(ulaw));
}

#if 0
void _ad_decode_ulaw(uint8_t *pcm8u_buf, uint8_t *ulaw_buf, uint16_t count)
{
	uint16_t i;
	for (i=0; i<count; i++)
	{
		pcm8u_buf[i] = _ad_pcm16s_to_pcm8u(_ad_ulaw2linear(ulaw_buf[i]));
	}
}
#endif

void ad_skip_to_clip(
	AudioDriver *ad, uint8_t stream_idx, SoundToken cur_token, SoundToken loop_token)
{
	assert(cur_token < (sizeof(audio_clips)/sizeof(AudioClip)));
	assert(loop_token < (sizeof(audio_clips)/sizeof(AudioClip)));
	AudioStream *as = &ad->stream[stream_idx];
	as->cur_token = cur_token;
	as->cur_offset = 0;
	as->loop_token = loop_token;
}

void ad_queue_loop_clip(AudioDriver *ad, uint8_t stream_idx, SoundToken loop_token)
{
	assert(loop_token < (sizeof(audio_clips)/sizeof(AudioClip)));
	AudioStream *as = &ad->stream[stream_idx];
	as->loop_token = loop_token;
}

