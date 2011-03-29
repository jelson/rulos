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

#ifndef _audio_driver_h
#define _audio_driver_h

#include "rocket.h"
#include "sound.h"
//#include "spiflash.h"

#define NUM_STREAMS	1
#define AUDIO_STREAM_BURST_EFFECTS		0
#define AUDIO_STREAM_CONTINUOUS_EFFECTS	1
#define AUDIO_STREAM_COUNTDOWN			2

#define AUDIO_RING_BUFFER_SIZE	50
	// 6.25ms

typedef struct s_audio_stream {
	SoundToken cur_token;
	uint32_t cur_offset;
	SoundToken loop_token;

	uint8_t _ring_buffer_storage[sizeof(RingBuffer)+1+AUDIO_RING_BUFFER_SIZE];
	RingBuffer *ring;
} AudioStream;

typedef struct s_audio_driver {
	// Streams tell what addresses to fetch, and carry ring buffers to
	// hold fetched data for compositing
	AudioStream stream[NUM_STREAMS];

	// Data comes from here
	//SPIFlash spif;

	// SPI read requests live here; we pass pointers in to SPIFlash.
	//SPIBuffer spibspace[2];
	// We toggle back and forth between spare spaces with this index
	uint8_t next_spib;

	// Points to hardware output buffer, which we fill from the compositing step
	RingBuffer *output_buffer;

#define TEST_OUTPUT_ONLY 0
#if TEST_OUTPUT_ONLY
	uint8_t test_output_val;
#endif // TEST_OUTPUT_ONLY

} AudioDriver;

void init_audio_driver(AudioDriver *ad);

void ad_skip_to_clip(
	AudioDriver *ad, uint8_t stream_idx, SoundToken cur_token, SoundToken loop_token);
	// Change immediately to cur_token, then play loop_token in a loop

void ad_queue_loop_clip(AudioDriver *ad, uint8_t stream_idx, SoundToken loop_token);
	// After current token finishes, start looping loop_token

#endif // _audio_driver_h
