#include <arpa/inet.h>	// htonl
#include "linear2ulaw.h"
#include "host_audio_capture.h"

void host_audio_init(HostAudio *ha)
{
	int rc;
	snd_pcm_hw_params_t *hw_params;
	char *device = "plughw:0,0";

	rc = snd_pcm_open(&ha->hdl, device, SND_PCM_STREAM_CAPTURE, 0);
	assert(rc>=0);

	rc = snd_pcm_hw_params_malloc(&hw_params);
	assert(rc>=0);

	rc = snd_pcm_hw_params_any(ha->hdl, hw_params);
	assert(rc>=0);

	rc = snd_pcm_hw_params_set_access(ha->hdl, hw_params,
		SND_PCM_ACCESS_RW_INTERLEAVED);
	assert(rc>=0);

	rc = snd_pcm_hw_params_set_format(ha->hdl, hw_params,
		SND_PCM_FORMAT_S16_LE);
	assert(rc>=0);

	unsigned int rate = 8000;
	rc = snd_pcm_hw_params_set_rate_near(ha->hdl, hw_params, &rate, 0);
	assert(rc>=0);

	rc = snd_pcm_hw_params_set_channels(ha->hdl, hw_params, 1);
	assert(rc>=0);

	rc = snd_pcm_hw_params(ha->hdl, hw_params);
	assert(rc>=0);

	snd_pcm_hw_params_free(hw_params);
	assert(rc>=0);
}

void host_audio_close(HostAudio *ha)
{
	snd_pcm_close(ha->hdl);
}

void host_audio_fetch_ulaw_samples(HostAudio *ha, uint8_t *ulaw_buf, int len)
{
	int rc;
	int16_t *pcm_buf = alloca(len*sizeof(int16_t));
	rc = snd_pcm_readi(ha->hdl, pcm_buf, len);
	assert(rc == len);

	linear2ulaw_buf(pcm_buf, ulaw_buf, len);
}

