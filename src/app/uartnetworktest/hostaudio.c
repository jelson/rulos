#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <alsa/asoundlib.h>
#include <arpa/inet.h>	// htonl
#include "linear2ulaw.h"
#include "aufile.h"

// http://www.equalarea.com/paul/alsa-audio.html#captureex

int16_t average(int16_t *buf, int n)
{
	int32_t sum = 0;
	int i;
	for (i=0; i<n; i++)
	{
		sum += buf[i];
	}
	sum /= n;
	return sum;
}

int main(int argc, char **argv)
{
	int rc;
	snd_pcm_t *hdl;
	snd_pcm_hw_params_t *hw_params;
	char *device = "plughw:0,0";

	rc = snd_pcm_open(&hdl, device, SND_PCM_STREAM_CAPTURE, 0);
	assert(rc>=0);

	rc = snd_pcm_hw_params_malloc(&hw_params);
	assert(rc>=0);

	rc = snd_pcm_hw_params_any(hdl, hw_params);
	assert(rc>=0);

	rc = snd_pcm_hw_params_set_access(hdl, hw_params,
		SND_PCM_ACCESS_RW_INTERLEAVED);
	assert(rc>=0);

	rc = snd_pcm_hw_params_set_format(hdl, hw_params,
		SND_PCM_FORMAT_S16_LE);
	assert(rc>=0);

	unsigned int rate = 8000;
	rc = snd_pcm_hw_params_set_rate_near(hdl, hw_params, &rate, 0);
	assert(rc>=0);

	rc = snd_pcm_hw_params_set_channels(hdl, hw_params, 1);
	assert(rc>=0);

	rc = snd_pcm_hw_params(hdl, hw_params);
	assert(rc>=0);

	snd_pcm_hw_params_free(hw_params);
	assert(rc>=0);

	FILE *fp = fopen("/tmp/mic.au", "w");

	AuHeader ah;
	ah.magic = htonl(AUDIO_FILE_MAGIC);
	ah.hdr_size = htonl(sizeof(ah));
	ah.data_size = htonl(AUDIO_UNKNOWN_SIZE);
	ah.encoding = htonl(AUDIO_FILE_ENCODING_MULAW_8);
	ah.sample_rate = htonl(8000);
	ah.channels = htonl(1);
	rc = fwrite(&ah, sizeof(ah), 1, fp);
	assert(rc == 1);
	while (1)
	{
#define NSAMPLES 128
		int16_t pcm_buf[NSAMPLES];
		rc = snd_pcm_readi(hdl, pcm_buf, NSAMPLES);
		assert(rc == NSAMPLES);

		int16_t avg = average(pcm_buf, NSAMPLES);
		fprintf(stderr, "avg %d\n", avg);

		uint8_t ulaw_buf[NSAMPLES];
		linear2ulaw_buf(pcm_buf, ulaw_buf, NSAMPLES);
		rc = fwrite(ulaw_buf, sizeof(ulaw_buf), 1, fp);
		assert(rc == 1);
	}

	fclose(fp);
	snd_pcm_close(hdl);
	return 0;
}
