#ifndef _AUDIO_OUT_H
#define _AUDIO_OUT_H

#include "rocket.h"

// 8 -> known good
// 7 -> sounds good
// 6 -> known bad
#define AO_BUFLENLG2	(8)
#define AO_BUFLEN		(((uint16_t)1)<<AO_BUFLENLG2)
#define AO_HALFBUFLEN	(((uint16_t)1)<<(AO_BUFLENLG2-1))
#define AO_BUFMASK		((((uint16_t)1)<<AO_BUFLENLG2)-1)
#define AO_HALFBUFMASK	((((uint16_t)1)<<(AO_BUFLENLG2-1))-1)

typedef struct
{
	uint8_t buffer[AO_BUFLEN];
	uint8_t index;
	uint8_t fill_index;
	Activation *fill_act;
} AudioOut;

void init_audio_out(AudioOut *ao, uint8_t timer_id, Activation *fill_act);

#endif // _AUDIO_OUT_H
