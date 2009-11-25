#ifndef _audio_driver_h
#define _audio_driver_h

#include "rocket.h"
#include "sound.h"
#include "spiflash.h"

typedef struct s_audio_driver {
	HalAudioRefillFunc func;
	SoundToken cur_token;
	uint32_t cur_offset;
	SoundToken loop_token;
	r_bool locked;

	SPIFlash spif;
	struct {
		ActivationFunc func;
		struct s_audio_driver *this;
	} spiComplete;
} AudioDriver;

void init_audio_driver(AudioDriver *ad);

void ad_skip_to_clip(AudioDriver *ad, SoundToken cur_token, SoundToken loop_token);
	// Change immediately to cur_token, then play loop_token in a loop

void ad_queue_loop_clip(AudioDriver *ad, SoundToken loop_token);
	// After current token finishes, start looping loop_token

#endif // _audio_driver_h
