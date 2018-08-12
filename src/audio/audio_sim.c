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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

#define min(a,b)	((a)<(b)?(a):(b))

#define AU_HEADER_SIZE 24
#define NUM_AUDIO_FILES 9

void setasync(int fd)
{
	int flags, rc;
	flags = fcntl(fd, F_GETFL);
	rc = fcntl(fd, F_SETFL, flags | O_ASYNC | O_NONBLOCK);
	assert(rc==0);
}

void printflags(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	fprintf(stderr, "flags for fd %d = %x\n", fd, flags);
}

typedef struct s_aufile {
	int auptr;
	int size;
	char *buf;
} AuFile;

void init_aufile(AuFile *auf, char *fn)
{
	FILE *src = fopen(fn, "r");
	if (src==NULL)
	{
		fprintf(stderr, "couldn't open %s\n", fn);
		exit(-1);
	}
	struct stat statbuf;
	fstat(fileno(src), &statbuf);
	auf->size = statbuf.st_size - AU_HEADER_SIZE;
	auf->buf = malloc(auf->size);
	assert(auf->buf!=NULL);
	auf->auptr = 0;
	int rc;
	rc = fread(auf->buf, 1, AU_HEADER_SIZE, src);	// skip au header and discard
	assert(rc==AU_HEADER_SIZE);
	rc = fread(auf->buf, 1, auf->size, src);
	assert(rc==auf->size);
	fclose(src);
	return;
}

// based on sample code at:
// http://www.speech.cs.cmu.edu/comp.speech/Section2/Q2.7.html
int16_t ulaw2linear(uint8_t ulawbyte)
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

uint8_t pcm16s_to_pcm8u(int16_t s)
{
	return (s>>8)+127;
}

#define DSP	1

main()
{
	setasync(0);

#if 0
	{
		int i;
		for (i=0; i<256; i++)
		{
			printf("decode(%2x) -> %d -> %2x\n",
				i, ulaw2linear(i), pcm16s_to_pcm8u(ulaw2linear(i)));
		}
		exit(0);
	}
#endif

	int audiofd;
#if DSP
	audiofd = open("/dev/dsp", O_ASYNC|O_WRONLY);
	setasync(audiofd);
	//audiofd = open("dsp.out", O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
#else	// mulaw
	audiofd = open("/dev/audio", O_ASYNC|O_WRONLY);
	setasync(audiofd);
#endif
	//audiofd = open("testout", O_CREAT|O_ASYNC|O_WRONLY);
	assert(audiofd>=0);

	AuFile aufiles[NUM_AUDIO_FILES];
	init_aufile(&aufiles[0], "golden-silence.au");
	init_aufile(&aufiles[1], "booster_start_8.au");
	init_aufile(&aufiles[2], "booster_running.au");
	init_aufile(&aufiles[3], "booster_flameout.au");
	init_aufile(&aufiles[4], "pong-wall.au");
	init_aufile(&aufiles[5], "pong-paddle.au");
	init_aufile(&aufiles[6], "pong-score.au");
	init_aufile(&aufiles[7], "bang-clong.au");
	init_aufile(&aufiles[8], "sawtooth0.8.au");
	int au_index = 0;
	int au_queued_next = 0;

	printflags(0);
	printflags(audiofd);
	int rc;
	
	while (1)
	{
		fd_set read_fds, write_fds, except_fds;
		FD_ZERO(&read_fds);
		FD_SET(0, &read_fds);
		FD_SET(0, &except_fds);
		FD_SET(audiofd, &write_fds);
		FD_SET(audiofd, &except_fds);
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		rc = select(audiofd+1, &read_fds, &write_fds, &except_fds, NULL);
		//fprintf(stderr, "select rc==%d\n", rc);
		
		if (FD_ISSET(0, &read_fds))
		{
			char input[1];
			rc = read(0, input, 1);
			if (rc>0)
			{
				assert(rc==1);
				fprintf(stderr, "INPUT: '%c'\n", input[0]);
				char c = input[0];
				if (c >= '0' && c <= '0'+NUM_AUDIO_FILES)
				{
					au_queued_next = c-'0';
				}
				else if (c=='s')
				{
					au_index = au_queued_next;
				}
				else if (c=='x')
				{
					au_index = au_queued_next;
					aufiles[au_index].auptr = 0;
					au_queued_next = 0;
				}
			}
		}

		if (FD_ISSET(audiofd, &write_fds))
		{
			AuFile *auf = &aufiles[au_index];
			int auchunks = 1024;
			int count = min(auchunks, auf->size-auf->auptr);
			if (count==0)
			{
				// loop the sound.
				au_index = au_queued_next;
				auf = &aufiles[au_index];
				auf->auptr = 0;
				count = min(auchunks, auf->size-auf->auptr);
			}
			char *outbuf;
#if DSP
			char buf[auchunks];
			outbuf = buf;
			int idx;
			for (idx=0; idx<count; idx++)
			{
				outbuf[idx] = pcm16s_to_pcm8u(ulaw2linear(auf->buf[auf->auptr+idx]));
				//printf("sample output = %d\n", outbuf[idx]);
			}
#else // mulaw
			outbuf = auf->buf+auf->auptr;
#endif
			rc = write(audiofd, outbuf, count);
			fprintf(stderr, "aufile %d (next %d) write auptr = %x audiofd rc = %d; errno= %d\n", au_index, au_queued_next, auf->auptr, rc, errno);
			if (rc!=count)
			{
				perror("write");
			}
			auf->auptr += rc;
		}
	}
}
