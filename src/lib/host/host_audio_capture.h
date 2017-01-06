#ifndef _HOST_AUDIO_CAPTURE_H
#define _HOST_AUDIO_CAPTURE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <alsa/asoundlib.h>


typedef struct
{
	snd_pcm_t *hdl;
} HostAudio;

void host_audio_init(HostAudio *ha);
void host_audio_close(HostAudio *ha);
void host_audio_fetch_ulaw_samples(HostAudio *ha, uint8_t *ulaw_buf, int len);

#endif // _HOST_AUDIO_CAPTURE_H
