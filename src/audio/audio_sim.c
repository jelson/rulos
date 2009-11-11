#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/select.h>

#include <errno.h>
#include <malloc.h>
#include <assert.h>

#define min(a,b)	((a)<(b)?(a):(b))

#define AU_HEADER_SIZE 24
#define NUM_AUDIO_FILES 8

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

main()
{
	setasync(0);

	int audiofd;
	audiofd = open("/dev/audio", O_ASYNC|O_WRONLY);
	//audiofd = open("testout", O_CREAT|O_ASYNC|O_WRONLY);
	assert(audiofd>=0);

	setasync(audiofd);

	AuFile aufiles[NUM_AUDIO_FILES];
	init_aufile(&aufiles[0], "silence.au");
	init_aufile(&aufiles[1], "booster_start_8.au");
	init_aufile(&aufiles[2], "booster_running.au");
	init_aufile(&aufiles[3], "booster_flameout.au");
	init_aufile(&aufiles[4], "pong-wall.au");
	init_aufile(&aufiles[5], "pong-paddle.au");
	init_aufile(&aufiles[6], "pong-score.au");
	init_aufile(&aufiles[7], "bang-clong.au");
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
			rc = write(audiofd, auf->buf+auf->auptr, count);
			fprintf(stderr, "aufile %d (next %d) write auptr = %x audiofd rc = %d; errno= %d\n", au_index, au_queued_next, auf->auptr, rc, errno);
			//perror("write");
			auf->auptr += rc;
		}
	}
}
