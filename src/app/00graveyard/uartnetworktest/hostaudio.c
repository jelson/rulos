#include <arpa/inet.h>	// htonl
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "graveyard/aufile.h"
#include "host/host_audio_capture.h"
#include "host/host_uart_network.h"
#include "host/linear2ulaw.h"

// http://www.equalarea.com/paul/alsa-audio.html#captureex

void *receive_thread(void *arg)
{
	HostUartNetwork *hun = (HostUartNetwork *) arg;
	while(1)
	{
		host_uart_receive(hun);
	}
}

AuHeader *make_header(void)
{
	static AuHeader ah;
	ah.magic = htonl(AUDIO_FILE_MAGIC);
	ah.hdr_size = htonl(sizeof(ah));
	ah.data_size = htonl(AUDIO_UNKNOWN_SIZE);
	ah.encoding = htonl(AUDIO_FILE_ENCODING_MULAW_8);
	ah.sample_rate = htonl(8000);
	ah.channels = htonl(1);
	return &ah;
}


#define NSAMPLES 32
int main(int argc, char **argv)
{
	HostAudio ha;
	host_audio_init(&ha);
#if TO_FILE
	int rc;
	FILE *fp = fopen("/tmp/mic.au", "w");
	rc = fwrite(make_header(), sizeof(AuHeader), 1, fp);
#else
	HostUartNetwork hun;
	host_uart_network_init(&hun, "/dev/ttyUSB0");

	pthread_t pt;
	pthread_create(&pt, NULL, receive_thread, &hun);
#endif

	while (1)
	{
		uint8_t ulaw_buf[NSAMPLES];
		host_audio_fetch_ulaw_samples(&ha, ulaw_buf, NSAMPLES);

#if TO_FILE
		rc = fwrite(ulaw_buf, sizeof(ulaw_buf), 1, fp);
		assert(rc == 1);
#else
		memcpy(hun.send_frame.buffer, ulaw_buf, sizeof(ulaw_buf));
		host_uart_network_send_buffer(&hun, AUDIO_ADDR, STREAMING_AUDIO_PORT, sizeof(ulaw_buf));
#endif
	}

#if TO_FILE
	fclose(fp);
#endif
	host_audio_close(&ha);

	return 0;
}
