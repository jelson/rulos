#ifndef _AUDIO_OUT_H
#define _AUDIO_OUT_H

#include "rocket.h"

// 7 -> known good
#define AO_BUFLENLG2	(5)
#define AO_BUFLEN		(((uint16_t)1)<<AO_BUFLENLG2)
#define AO_BUFMASK		((((uint16_t)1)<<AO_BUFLENLG2)-1)
#define AO_NUMBUFS		2

typedef struct
{
	uint8_t buffers[AO_NUMBUFS][AO_BUFLEN];
	uint8_t sample_index;
	uint8_t play_buffer;

	uint8_t fill_buffer;	// the one fill_act should populate when called.
	Activation *fill_act;
} AudioOut;

void init_audio_out(AudioOut *ao, uint8_t timer_id, Activation *fill_act);

#endif // _AUDIO_OUT_H
