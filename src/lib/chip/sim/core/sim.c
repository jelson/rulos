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

/*
 * These are the base simulator modules, for just the clock and TWI.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "chip/sim/core/sim.h"
#include "core/clock.h"
#include "core/heap.h"
#include "core/logging.h"
#include "core/media.h"
#include "core/util.h"
#include "hardware_types.h"

uint32_t f_cpu = 4000000;
uint8_t hal_initted = 0;

/**************** clock ****************/

sigset_t mask_set;

#define MAX_HANDLERS 20

typedef struct {
  Handler func;
  void *data;
} SimActivation_t;

static SimActivation_t simClockHandlers[MAX_HANDLERS];
static int numSimClockHandlers = 0;

static SimActivation_t simSIGIOHandlers[MAX_HANDLERS];
static int numSimSIGIOHandlers = 0;

static void sim_register_generic_handler(SimActivation_t *handlerList,
                                         int *numHandlers, Handler func,
                                         void *data) {
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  handlerList[*numHandlers].func = func;
  handlerList[*numHandlers].data = data;
  (*numHandlers)++;
  hal_end_atomic(old_interrupts);
}

void sim_register_clock_handler(Handler func, void *data) {
  sim_register_generic_handler(simClockHandlers, &numSimClockHandlers, func,
                               data);
}

void sim_register_sigio_handler(Handler func, void *data) {
  sim_register_generic_handler(simSIGIOHandlers, &numSimSIGIOHandlers, func,
                               data);
}

static void sim_generic_fire_handlers(SimActivation_t *handlerList,
                                      int numHandlers) {
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  int i;

  for (i = 0; i < numHandlers; i++) {
    handlerList[i].func(handlerList[i].data);
  }
  hal_end_atomic(old_interrupts);
}

static void sim_clock_handler(int signo) {
  sim_generic_fire_handlers(simClockHandlers, numSimClockHandlers);
}

static void sim_sigio_handler(int signo) {
  sim_generic_fire_handlers(simSIGIOHandlers, numSimSIGIOHandlers);
}

uint32_t hal_start_clock_us(uint32_t us, Handler handler, void *data,
                            uint8_t timer_id) {
  assert(hal_initted == HAL_MAGIC);  // did you forget to call hal_init()?

  /* init clock stuff */
  sigemptyset(&mask_set);
  sigaddset(&mask_set, SIGALRM);
  sigaddset(&mask_set, SIGIO);

  struct itimerval ivalue, ovalue;
  ivalue.it_interval.tv_sec = us / 1000000;
  ivalue.it_interval.tv_usec = (us % 1000000);
  ivalue.it_value = ivalue.it_interval;
  setitimer(ITIMER_REAL, &ivalue, &ovalue);

  sim_register_clock_handler(handler, data);
  signal(SIGALRM, sim_clock_handler);

  return us;
}

// this COULD be implemented with gettimeofday(), but I'm too lazy,
// since the only reason this function exists is for the wall clock
// app, so it only matters in hardware.
uint16_t hal_elapsed_milliintervals() { return 0; }

void hal_speedup_clock_ppm(int32_t ratio) {
  // do nothing for now
}

rulos_irq_state_t is_blocked = 0;

rulos_irq_state_t hal_start_atomic() {
  sigprocmask(SIG_BLOCK, &mask_set, NULL);
  rulos_irq_state_t retval = is_blocked;
  is_blocked = 1;
  return retval;
}

void hal_end_atomic(rulos_irq_state_t blocked) {
  if (!blocked) {
    is_blocked = 0;
    sigprocmask(SIG_UNBLOCK, &mask_set, NULL);
  }
}

void hal_idle() {
  // turns out 'man sleep' says sleep & sigalrm don't mix. yield is what we
  // want. No, sched_yield doesn't wait ANY time. libc suggests select()
  static struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  select(0, NULL, NULL, NULL, &tv);
}

void hal_delay_ms(uint16_t ms) {
  static struct timeval tv;
  tv.tv_sec = ms / 1000;
  tv.tv_usec = 1000 * (ms % 1000);
  select(0, NULL, NULL, NULL, &tv);
}

/********************** sensors *************************/

Handler sensor_interrupt_handler = NULL;
void *sensor_interrupt_data = NULL;
const uint32_t sensor_interrupt_simulator_counter_period = 507;
uint32_t sensor_interrupt_simulator_counter;

void sim_sensor_poll(void *data) {
  sensor_interrupt_simulator_counter += 1;
  if (sensor_interrupt_simulator_counter ==
      sensor_interrupt_simulator_counter_period) {
    if (sensor_interrupt_handler != NULL) {
      sensor_interrupt_handler(sensor_interrupt_data);
    }
    sensor_interrupt_simulator_counter = 0;
  }
}

void sensor_interrupt_register_handler(Handler handler, void *data) {
  sensor_interrupt_handler = handler;
  sensor_interrupt_data = data;
  sim_register_clock_handler(sim_sensor_poll, NULL);
}

/*************** twi ************************/

typedef struct {
  MediaStateIfc media;
  r_bool initted;
  MediaRecvSlot *mrs;
  int udp_socket;
} SimTwiState;
SimTwiState g_sim_twi_state = {{NULL}, FALSE};

static void sim_twi_send(MediaStateIfc *media, Addr dest_addr,
                         const unsigned char *data, uint8_t len,
                         MediaSendDoneFunc sendDoneCB, void *sendDoneCBData);
static void sim_twi_poll(void *data);

MediaStateIfc *hal_twi_init(uint32_t speed_khz, Addr local_addr,
                            MediaRecvSlot *mrs) {
  SimTwiState *twi_state = &g_sim_twi_state;
  twi_state->media.send = sim_twi_send;
  twi_state->mrs = mrs;
  twi_state->udp_socket = socket(PF_INET, SOCK_DGRAM, 0);

  int rc;
  int on = 1;
#ifdef O_ASYNC
  int flags = fcntl(twi_state->udp_socket, F_GETFL);
  rc = fcntl(twi_state->udp_socket, F_SETFL, flags | O_ASYNC);
  assert(rc == 0);
#endif
  rc = ioctl(twi_state->udp_socket, FIOASYNC, &on);
  assert(rc == 0);
  rc = ioctl(twi_state->udp_socket, FIONBIO, &on);
  assert(rc == 0);
#ifdef SIOCSPGRP
  int flag = -getpid();
  rc = ioctl(twi_state->udp_socket, SIOCSPGRP, &flag);
  assert(rc == 0);
#endif

  struct sockaddr_in sai;
  sai.sin_family = AF_INET;
  sai.sin_addr.s_addr = htonl(INADDR_ANY);
  sai.sin_port = htons(SIM_TWI_PORT_BASE + local_addr);
  bind(twi_state->udp_socket, (struct sockaddr *)&sai, sizeof(sai));
  twi_state->initted = TRUE;
  sim_register_clock_handler(sim_twi_poll, NULL);
  sim_register_sigio_handler(sim_twi_poll, NULL);
  return &twi_state->media;
}

static void sim_twi_poll(void *data) {
  SimTwiState *twi_state = &g_sim_twi_state;
  if (!twi_state->initted) {
    return;
  }

  // On some OSs (cough cough, cygwin) it seems that non-blocking UDP socket
  // reads simply don't work. After much tinkering I could not get recv() to
  // return EAGAIN. So instead we poll with select().
  static struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  fd_set set;
  FD_ZERO(&set);
  FD_SET(twi_state->udp_socket, &set);
  assert(0 == select(twi_state->udp_socket+1, &set, NULL, NULL, &tv));
  if (!FD_ISSET(twi_state->udp_socket, &set)) {
    return;
  }

  char buf[4096];
  int rc = recv(twi_state->udp_socket, buf, sizeof(buf), MSG_DONTWAIT);

  if (rc < 0) {
    assert(errno == EAGAIN);
    return;
  }

  assert(rc != 0);

  MediaRecvSlot *const mrs = twi_state->mrs;
  if (mrs->packet_len > 0) {
    LOG("TWI SIM: Packet arrived but network stack buffer busy; dropping");
    return;
  }

  if (rc > mrs->capacity) {
    LOG("TWI SIM: Discarding %d-byte packet; too long for net stack's buffer",
        rc);
    return;
  }

  mrs->packet_len = rc;
  memcpy(mrs->data, buf, rc);
  mrs->func(mrs);
  mrs->packet_len = 0;
}

typedef struct {
  MediaSendDoneFunc sendDoneCB;
  void *sendDoneCBData;
} sendCallbackAct_t;

static void sim_twi_send(MediaStateIfc *media, Addr dest_addr,
                         const unsigned char *data, uint8_t len,
                         MediaSendDoneFunc sendDoneCB, void *sendDoneCBData) {
  SimTwiState *twi_state = (SimTwiState *)media;

#if 0
	LOG("hal_twi_send_byte(%02x [%c])",
		byte,
		(byte>=' ' && byte<127) ? byte : '_');
#endif

  struct sockaddr_in sai;

  sai.sin_family = AF_INET;
  sai.sin_addr.s_addr = htonl(0x7f000001);
  sai.sin_port = htons(SIM_TWI_PORT_BASE + dest_addr);
  sendto(twi_state->udp_socket, data, len, 0, (struct sockaddr *)&sai,
         sizeof(sai));

  if (sendDoneCB != NULL) {
    schedule_now((ActivationFuncPtr)sendDoneCB, sendDoneCBData);
  }
}

/************ init ***********************/

static FILE *logfp = NULL;
static uint64_t init_time = 0;

uint64_t curr_time_usec() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((uint64_t)1000000) * tv.tv_sec + tv.tv_usec;
}

void sim_log(const char *fmt, ...) {
  va_list ap;
  char message[4096];

  va_start(ap, fmt);
  vsnprintf(message, sizeof(message), fmt, ap);
  va_end(ap);

  uint64_t normalized_time_usec = curr_time_usec() - init_time;

  fprintf(logfp, "%" PRIu64 ".%06" PRIu64 ": %s",
          normalized_time_usec / 1000000, normalized_time_usec % 1000000,
          message);
  fflush(logfp);
}

void hal_init() {
  logfp = fopen("log", "w");
  init_time = curr_time_usec();
  signal(SIGIO, sim_sigio_handler);
  hal_initted = HAL_MAGIC;
}
