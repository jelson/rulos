/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
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
#include <stdbool.h>
#include <signal.h>

#define NUM_AUDIO_FILES 9

// input
char* music_buffer;
int buffer_size;

int played_offset;
bool done = false;

// output
int audiofd;

void push_audio() {
  while (true) {
    char* ptr = &music_buffer[played_offset];
    int len = buffer_size - played_offset;
    int rc = write(audiofd, ptr, len);
    if (rc < 0) {
      if (errno == EAGAIN) {
        break;
      } else {
        fprintf(stderr, "Error %d on write\n", errno);
        break;
      }
    } else if (rc > len) {
      assert(false);
    } else {
      played_offset += rc;
    }
  }
  done = played_offset == buffer_size;
}

void sigio_handler(int signo) {
  push_audio();
}

void read_all(FILE* fp, char* ptr, int size) {
  int rc;
  while (size > 0) {
    rc = fread(ptr, 1, size, fp);
    assert(0 < rc);
    ptr += rc;
    size -= rc;
  }
}

int main() {
  int rc;

  // Load the music buffer.
  FILE *fp;
  fp = fopen("sdcard/music/funky-town.raw", "rb");
  fseek(fp, 0, SEEK_END);
  buffer_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  music_buffer = malloc(buffer_size);
//  rc = fread(music_buffer, buffer_size, 1, fp);
  read_all(fp, music_buffer, buffer_size);
  fclose(fp);
  played_offset = 0;

  // Open the audio path
  int pipefd[2];
  rc = pipe2(pipefd, /*flags*/ 0);
  fprintf(stderr, "XXX i2s readpipe %d writepipe %d", pipefd[0], pipefd[1]);
  assert(rc==0);
  int pid = fork();
  if (pid == 0) {
    // child
    // close "all" the other fds, so we're not getting SIGIO on network packets and such.
    for (int fdi=3; fdi<1000; fdi++) {
      if (fdi != pipefd[0]) {
        close(fdi);
      }
    }
    dup2(pipefd[0], 0); // read end of pipe becomes stdin
    char* argv[] = {"aplay", "-f", "S16_LE", "--file-type", "raw", "--rate", "50000", "--channels", "2", "-", NULL};
    execv("/usr/bin/aplay", argv);
    assert(false);
    exit(-1);
  }
  // Parent.
  close(pipefd[0]);
  audiofd = pipefd[1];
  assert(audiofd >= 0);
  int flags = fcntl(audiofd, F_GETFL);
  rc = fcntl(audiofd, F_SETFL, flags | O_ASYNC | O_NONBLOCK);
  assert(rc == 0);
  rc = fcntl(audiofd, F_SETOWN, getpid());
  assert(rc == 0);

  // Set up signal handler
  signal(SIGIO, sigio_handler);

  // Start the flow of audio
  push_audio();

  // sleep and let SIGIO do the work.
  while (!done) {
    sleep(1);
  }
}
