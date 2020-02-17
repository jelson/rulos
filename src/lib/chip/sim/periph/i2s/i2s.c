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
#include <fcntl.h>
#include <unistd.h>
#include <signal.h> // XXX yuck
#include <errno.h>

#include "periph/i2s/hal_i2s.h"

static void set_async(int fd);
static void sim_audio_output_poll(void *data);
static void poke_fd();
static void notify_application_trampoline(void* v_done_index);

static struct {
  hal_i2s_play_done_cb_t play_done_cb;
  void* user_data;

  int16_t* samples;
  uint16_t num_samples_per_halfbuffer;

  uint16_t next_buffer_index;
  uint16_t next_write_offset_bytes; // how many bytes of next_buffer_index have already been emitted

  int audiofd;
} sim_i2s;

void hal_i2s_condition_buffer(int16_t* samples, uint16_t num_samples) {
    LOG("SIM hal_i2s_condition_buffer");
}

// Fake a sigio periodically in case we ... lose one!??
// Trying to understand a situation where write gives EAGAIN but SIGIO doesn't follow.
static void fake_sigio(void* context)
{
  sim_audio_output_poll(NULL);
  schedule_us(1000000, fake_sigio, NULL);
}

void hal_i2s_init(uint16_t sample_rate, hal_i2s_play_done_cb_t play_done_cb,
                  void* user_data) {
  LOG("SIM hal_i2s_init");
  sim_i2s.play_done_cb = play_done_cb;
  sim_i2s.user_data = user_data;

  sim_i2s.samples = NULL;

  int pipefd[2];
  int rc = pipe2(pipefd, /*flags*/ 0);
  LOG("XXX i2s readpipe %d writepipe %d", pipefd[0], pipefd[1]);
  assert(rc==0);
  int pid = fork();
  if (pid == 0) {
    // child

    // XXX yuck
    sigset_t mask_set;
    sigemptyset(&mask_set);
    sigaddset(&mask_set, SIGIO);
    sigprocmask(SIG_BLOCK, &mask_set, NULL);

    // close "all" the other fds, so we're not getting SIGIO on network packets and such.
    for (int fdi=3; fdi<1000; fdi++) {
      if (fdi != pipefd[0]) {
        close(fdi);
      }
    }
    dup2(pipefd[0], 0); // read end of pipe becomes stdin
    char* argv[] = {"aplay", "--file-type", "raw", "--rate", "50000", "--channels", "2", "-", NULL};
    execv("/usr/bin/aplay", argv);
    assert(false);
    exit(-1);
  }
  // Parent.
  close(pipefd[0]);
  //sim_i2s.audiofd = open("/dev/dsp", O_ASYNC | O_WRONLY);
  sim_i2s.audiofd = pipefd[1];
  assert(sim_i2s.audiofd >= 0);
  set_async(sim_i2s.audiofd);

  sim_register_sigio_handler(sim_audio_output_poll, NULL);
  schedule_us(1000000, fake_sigio, NULL);
}

void hal_i2s_start(int16_t* samples, uint16_t num_samples_per_halfbuffer) {
  LOG("SIM hal_i2s_start");
  sim_i2s.samples = samples;
  sim_i2s.num_samples_per_halfbuffer = num_samples_per_halfbuffer;
  sim_i2s.next_buffer_index = 0;
  sim_i2s.next_write_offset_bytes = 0;

  poke_fd();
}

static void sim_audio_output_poll(void *data) {
  LOG("SIM wakes on SIGIO");
  poke_fd();
}

// Something has changed (start or a SIGIO). Try to push out another halfbuffer.
static void poke_fd() {
  if (sim_i2s.samples == NULL) {
    return;
  }

  int byte_count = sim_i2s.num_samples_per_halfbuffer * sizeof(int16_t) - sim_i2s.next_write_offset_bytes;
  char* ptr = ((char*) &sim_i2s.samples[sim_i2s.next_buffer_index * sim_i2s.num_samples_per_halfbuffer])
    + sim_i2s.next_write_offset_bytes;
  int rc = write(sim_i2s.audiofd, ptr, byte_count);
  LOG("Poke samples %p samplesPerHB %08x next_idx %02x next-write-offset %04x -> write took %04x bytes",
    sim_i2s.samples, sim_i2s.num_samples_per_halfbuffer,
    sim_i2s.next_buffer_index, sim_i2s.next_write_offset_bytes,
    rc);
  if (rc == byte_count) {
    LOG("XXX audio_fd full write");
    // half-buffer is done!
    // Flop the buffers
    uint8_t buffer_done_index = sim_i2s.next_buffer_index;
    sim_i2s.next_buffer_index = 1 - sim_i2s.next_buffer_index;
    sim_i2s.next_write_offset_bytes = 0;
    // Notify the application, but only after this call stack finishes
    // (so the periph/i2s state machine can finish updating its own state).
    schedule_now(notify_application_trampoline, (void*)(long int) buffer_done_index);
  } else if (rc < 0) {
    // error return
    if (errno == EAGAIN) {
      // buffer's full! No state change; expect SIGIO to wake us up.
      LOG("XXX audio_fd EAGAIN");
    } else {
      assert(false);  // write failed
    }
  } else if (rc > byte_count) {
    assert(false);   // outside of write()'s spec.
  } else {
    assert(0 <= rc);
    assert(rc < byte_count);
    LOG("XXX audio_fd short write");
    sim_i2s.next_write_offset_bytes -= rc;
    assert(0 <= sim_i2s.next_write_offset_bytes);
    // Wait for a SIGIO to write more bytes.
  }
}

static void notify_application_trampoline(void* v_done_index) {
  int done_index = (int) v_done_index;
  sim_i2s.play_done_cb(sim_i2s.user_data, done_index);
}

void hal_i2s_stop() {
  LOG("SIM hal_i2s_stop");
  // Can't pull back any buffered bytes, but can tell poke_fd to stop poking.
  sim_i2s.samples = NULL;
}

static void set_async(int fd) {
  int flags, rc;
  flags = fcntl(fd, F_GETFL);
  rc = fcntl(fd, F_SETFL, flags | O_ASYNC | O_NONBLOCK);
  assert(rc == 0);
}

