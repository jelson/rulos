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
  FILE* debug_teefp;  // capture audio stream for debugging

  bool outstanding_fill_requests[2];
} sim_i2s;

void hal_i2s_condition_buffer(int16_t* samples, uint16_t num_samples) {
    LOG("SIM hal_i2s_condition_buffer");
}

// Fake a sigio periodically in case we ... lose one!??
// Trying to understand a situation where write gives EAGAIN but SIGIO doesn't follow.
// May 2021: DenverCoder9(Jer) conjectures that, in the simulator, while
// initially filling aplay's buffers, it takes longer to load data than
// to "play" it, so the refill process (somehow) doesn't trigger playing
// the next buffer. Sending fake sigios frequently enough is sufficient
// to bump us out of this state.
// There's probably a state machine bug or flow interaction with the
// higher layers we could fix.
static void fake_sigio(void* context)
{
  sim_audio_output_poll(NULL);
  schedule_us(10000, fake_sigio, NULL);
}

void hal_i2s_init(uint16_t sample_rate, hal_i2s_play_done_cb_t play_done_cb,
                  void* user_data) {
  LOG("SIM hal_i2s_init");
  sim_i2s.play_done_cb = play_done_cb;
  sim_i2s.user_data = user_data;

  sim_i2s.samples = NULL;
  sim_i2s.outstanding_fill_requests[0] = false;
  sim_i2s.outstanding_fill_requests[1] = false;

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
    const char* const argv[] = {"aplay", "-f", "S16_LE", "--file-type", "raw", "--rate", "50000", "--channels", "2", "-", NULL};
    execv("/usr/bin/aplay", (char* const*) argv);
    assert(false);
    exit(-1);
  }
  // Parent.
  close(pipefd[0]);
  //sim_i2s.audiofd = open("/dev/dsp", O_ASYNC | O_WRONLY);
  sim_i2s.audiofd = pipefd[1];
  assert(sim_i2s.audiofd >= 0);
  set_async(sim_i2s.audiofd);

  sim_i2s.debug_teefp = NULL;
#if 0
  sim_i2s.debug_teefp = fopen("audio-tee.rawaudio", "wb");
  assert(sim_i2s.debug_teefp != NULL);
#endif

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
  //LOG("SIM wakes on SIGIO");
  poke_fd();
}

static void request_refill(int buffer_idx) {
  if (sim_i2s.outstanding_fill_requests[buffer_idx]) {
    LOG("XXX coalescing overlapping fill request for %d", buffer_idx);
    return;
  }
  LOG("XXX poke_now upcall fill buf %d", buffer_idx);
  sim_i2s.outstanding_fill_requests[buffer_idx] = true;
  schedule_now(notify_application_trampoline, (void*)(long int) buffer_idx);
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

  if (sim_i2s.debug_teefp != NULL && rc > 0) {
    // tee away however many bytes went to real audio pipe.
    int rc2 = fwrite(ptr, rc, 1, sim_i2s.debug_teefp);
    fflush(sim_i2s.debug_teefp);
    assert(rc2==1);
  }

  if (rc == byte_count) {
    LOG("XXX audio_fd full write");
    // half-buffer is done!
    // Flop the buffers
    uint8_t buffer_done_index = sim_i2s.next_buffer_index;
    sim_i2s.next_buffer_index = 1 - sim_i2s.next_buffer_index;
    sim_i2s.next_write_offset_bytes = 0;
    // Notify the application, but only after this call stack finishes
    // (so the periph/i2s state machine can finish updating its own state).
    request_refill(buffer_done_index);
  } else if (rc < 0) {
    // error return
    if (errno == EAGAIN) {
      // buffer's full! No state change; expect SIGIO to wake us up.
      LOG("XXX audio_fd EAGAIN");
    } else {
      perror("Write failed to audio fd");
      assert(false);  // write failed
    }
  } else if (rc > byte_count) {
    assert(false);   // outside of write()'s spec.
  } else {
    assert(0 <= rc);
    assert(rc < byte_count);
    LOG("XXX audio_fd short write");
    sim_i2s.next_write_offset_bytes += rc;
    assert(0 <= sim_i2s.next_write_offset_bytes);
    // Wait for a SIGIO to write more bytes.
  }
}

static void notify_application_trampoline(void* v_done_index) {
  int done_index = (size_t) v_done_index;
  sim_i2s.outstanding_fill_requests[done_index] = false;
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
  // Send SIGIO
  rc = fcntl(fd, F_SETFL, flags | O_ASYNC | O_NONBLOCK);
  assert(rc == 0);
  // Send signals to this process.
  rc = fcntl(fd, F_SETOWN, getpid());
  assert(rc == 0);
}

