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

#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "chip/sim/core/sim.h"
#include "core/rulos.h"
#include "core/util.h"
#include "periph/ring_buffer/rocket_ring_buffer.h"
#include "periph/uart/uart.h"

/***************** audio ********************/

#define AUDIO_BUF_SIZE 30
typedef struct s_SimAudioState {
  int audiofd;
  int flowfd;
  RingBuffer *ring;
  uint8_t _storage[sizeof(RingBuffer) + 1 + AUDIO_BUF_SIZE];
  int write_avail;
} SimAudioState;
SimAudioState alloc_simAudioState, *simAudioState = NULL;

static void sim_audio_poll(void *data);

void setnonblock(int fd) {
  int flags, rc;
  flags = fcntl(fd, F_GETFL);
  rc = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  assert(rc == 0);
}

void start_audio_fork_shuttling_child(SimAudioState *sas);
void audio_shuttling_child(int audiofd, int flowfd);

void hal_audio_init() {
  assert(simAudioState == NULL);  // duplicate initialization
  simAudioState = &alloc_simAudioState;
  simAudioState->ring = (RingBuffer *)simAudioState->_storage;
  init_ring_buffer(simAudioState->ring, sizeof(simAudioState->_storage));
  simAudioState->write_avail = 0;

  start_audio_fork_shuttling_child(simAudioState);

  sim_register_clock_handler(sim_audio_poll, NULL);
  sim_register_sigio_handler(sim_audio_poll, NULL);
}

void start_audio_fork_shuttling_child(SimAudioState *sas) {
  // Now, you'd think that you could just open /dev/dsp with O_NONBLOCK
  // and just write to it as it accepts bytes, right? You'd think that,
  // but then you'd be wrong, see, because it just doesn't work right;
  // it produces blips and pops that sound like gaps in the stream.
  // I tried all sorts of variations, from select(timeout=NULL), to
  // select(timeout=.5ms), to busy-waiting, but you just can't seem to
  // get the bytes in fast enough with O_NONBLOCK to keep the driver
  // full. Perhaps the audio driver has a broken implementation of
  // nonblocking-mode file descriptors? I do not know; all I know is
  // that the "obvious thing" sure isn't working.
  //
  // So my apalling (apollo-ing?) workaround: fork a child process
  // to read from a pipe and patiently feed the bytes into a nonblocking
  // fd to /dev/dsp. Yes. It works.
  //
  // Of course, pipes have something like 32k of buffer, which introduces
  // an unacceptably-long 4 sec latency in the audio stream, on top of the
  // already-too-long-but-non-negotiable .25-sec latency due to the 1k
  // buffer in /dev/dsp.
  //
  // So my even-more-apalling (gemini-ing?) workaroundaround: have the
  // child process signal flow control by sending bytes on an *upstream*
  // pipe; each upstream byte means there's room for another downstream
  // byte. Sheesh.

  int rc;
  int audiofds[2];
  rc = pipe(audiofds);
  assert(rc == 0);
  int flowfds[2];
  rc = pipe(flowfds);
  assert(rc == 0);

  int pid = fork();
  assert(pid >= 0);

  if (pid == 0) {
    // child
    audio_shuttling_child(audiofds[0], flowfds[1]);
    assert(FALSE);  // should not return
  }

  simAudioState->audiofd = audiofds[1];
  setnonblock(simAudioState->audiofd);
  simAudioState->flowfd = flowfds[0];
  setnonblock(simAudioState->flowfd);
}

int open_audio_device() {
#if 0
	int devdspfd = open("/dev/dsp", O_WRONLY, 0);
	return devdspfd;
#else
  int pipes[2];
  int rc = pipe(pipes);
  assert(rc == 0);
  pid_t pid = fork();
  if (pid == 0) {
    // child
    close(0);
    close(1);
    close(2);
    close(pipes[1]);
    dup2(pipes[0], 0);
    rc = system("aplay");
    assert(false);
  } else {
    // parent
    close(pipes[0]);
    return pipes[1];
  }
  assert(false);
  return -1;
#endif
}

void audio_shuttling_child(int audiofd, int flowfd) {
  char flowbuf = 0;
  char audiobuf[6];
  int rc;

  int devdspfd = open_audio_device();
  assert(devdspfd >= 0);
  int debugfd = open("devdsp", O_CREAT | O_WRONLY, S_IRWXU);
  assert(debugfd >= 0);

  // give buffer a little depth.
  // Smaller is better (less latency);
  // but you need a minimum amount. In audioboard, we run the
  // system clock at 1kHz (1ms). The audio subsystem is polled off
  // the system clock, so we need to be able to fill at least 8 samples
  // every poll to keep up with the buffer's drain rate.
  // Experimentation shows that 10 is the minimum value that avoids
  // clicks and pops.
  int i;
  for (i = 0; i < 12; i++) {
    rc = write(flowfd, &flowbuf, 1);
    assert(rc == 1);
  }

  while (1) {
    // loop doing blocking reads and, more importantly,
    // writes, which seem to stitch acceptably on /dev/dsp
    rc = read(audiofd, audiobuf, 1);
    assert(rc == 1);
    rc = write(devdspfd, audiobuf, 1);
    assert(rc == 1);
    rc = write(debugfd, audiobuf, 1);
    assert(rc == 1);

    // release a byte of flow control
    rc = write(flowfd, &flowbuf, 1);
    assert(rc == 1);
  }
}

void debug_audio_rate() {
  static int counter = 0;
  static struct timeval start_time;
  if (counter == 0) {
    gettimeofday(&start_time, NULL);
  }
  counter += 1;
  if (counter == 900) {
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    suseconds_t tv_usec = end_time.tv_usec - start_time.tv_usec;
    time_t tv_sec = end_time.tv_sec - start_time.tv_sec;
    tv_usec += tv_sec * 1000000;
    float rate = 900.0 / tv_usec * 1000000.0;
    LOG("output rate %f samples/sec", rate);
    counter = 0;
  }
}

static void sim_audio_poll(void *data) {
  int rc;

  if (simAudioState == NULL) {
    return;
  }

  // check for flow control signal
  char flowbuf[10];
  rc = read(simAudioState->flowfd, flowbuf, sizeof(flowbuf));
  if (rc >= 0) {
    simAudioState->write_avail += rc;
  }

  while (simAudioState->write_avail > 0 &&
         ring_remove_avail(simAudioState->ring) > 0) {
    uint8_t sample = ring_remove(simAudioState->ring);
    // LOG("sim_audio_poll removes sample %2x", sample);
    int wrote = write(simAudioState->audiofd, &sample, 1);
    assert(wrote == 1);

    simAudioState->write_avail -= 1;

    debug_audio_rate();
  }
}

void hal_audio_fire_latch(void) {}

void hal_audio_shift_sample(uint8_t sample) {}
