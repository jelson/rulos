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

#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "core/rulos.h"
#ifndef SIM
#include "core/hardware.h"
#endif

#define REGISTER_SER GPIO_D5  // serial data input
#define REGISTER_LAT GPIO_D6  // assert register to parallel output lines
#define REGISTER_CLK GPIO_D7  // shift in data to register

#define SAMPLE_RATE_HZ 12000
#define USEC_PER_SAMPLE (1000000 / SAMPLE_RATE_HZ)
#define MAX_SAMPLES 250

#define NOTE_LEN 500000

#ifndef SIM
static void init_audio_pins() {
  gpio_make_output(REGISTER_SER);
  gpio_make_output(REGISTER_LAT);
  gpio_make_output(REGISTER_CLK);
  gpio_clr(REGISTER_LAT);
  gpio_clr(REGISTER_CLK);
}

static inline void shift_out_8bits(uint8_t num) {
  int i;

  for (i = 0; i < 8; i++) {
    gpio_set_or_clr(REGISTER_SER, num & 0x1);
    gpio_set(REGISTER_CLK);
    gpio_clr(REGISTER_CLK);
    num >>= 1;
  }
}

static inline void latch_output() {
  gpio_set(REGISTER_LAT);
  gpio_clr(REGISTER_LAT);
}
#else  // sim

static inline void shift_out_8bits(uint8_t num) { LOG("%d,\n", num); }

static inline void latch_output() {}
static void init_audio_pins() {}
#endif

typedef struct {
  uint8_t waveform[MAX_SAMPLES];
  uint16_t num_samples;
  uint16_t sample_idx;
} waveformAct_t;

void emit_waveform(waveformAct_t *wa) {
  // assert the previous value.  We do this at the beginning to
  // reduce jitter.
  latch_output();

#ifdef USE_SCHEDULER_FOR_WAVEFORM_PLAYBACK
  schedule_us(USEC_PER_SAMPLE, (ActivationFuncPtr)emit_waveform, wa);
#endif

  if (wa->num_samples == 0) {
    shift_out_8bits(0);
    return;
  }

  wa->sample_idx = (wa->sample_idx + 1) % wa->num_samples;
  shift_out_8bits(wa->waveform[wa->sample_idx]);
}

void start_frequency(waveformAct_t *ta, float frequency) {
  LOG("starting frequency %f\n", frequency);

  // silence
  if (frequency == 0) {
    ta->num_samples = 0;
    return;
  }

  uint16_t num_samples = SAMPLE_RATE_HZ / frequency;

  if (num_samples > MAX_SAMPLES) return;

  uint16_t i;
  for (i = 0; i < num_samples; i++)
    ta->waveform[i] =
        128 +
        (128.0 * sin(i * frequency * 2 * 3.14159 / (1.0 * SAMPLE_RATE_HZ)));
  ta->num_samples = num_samples;
  ta->sample_idx = 0;
}

typedef struct {
  waveformAct_t *wa;
  int i;
} changeFrequencyAct_t;

void change_frequency(changeFrequencyAct_t *cfa) {
  float scale[] = {261.63,  // c4
                   293.66,  // d4
                   329.63,  // e4
                   349.23,  // f4
                   392.00,  // g4
                   440.00,  // a4
                   493.88,  // b4
                   523.25,  // c5
                   0        // rest
                   ,
                   -1};

  cfa->i++;
  if (scale[cfa->i] == -1) cfa->i = 0;

  start_frequency(cfa->wa, scale[cfa->i]);
  schedule_us(NOTE_LEN, (ActivationFuncPtr)change_frequency, cfa);
}

waveformAct_t wa;
changeFrequencyAct_t cfa;

int main() {
  hal_init();
  init_clock(100000, TIMER1);

  init_audio_pins();

  memset(&wa, 0, sizeof(wa));
  start_frequency(&wa, 0);

  cfa.wa = &wa;
  cfa.i = -1;
  schedule_now((ActivationFuncPtr)change_frequency, &cfa);

#ifdef USE_SCHEDULER_FOR_WAVEFORM_PLAYBACK
  schedule_now((Activation *)&wa);
#else
  hal_start_clock_us(USEC_PER_SAMPLE, (Handler)emit_waveform, &wa, TIMER2);
#endif

#if 0
	wa.waveform[0] = 179;
	wa.waveform[1] = 221;
	wa.waveform[2] = 248;
	wa.waveform[3] = 255;
	wa.waveform[4] = 241;
	wa.waveform[5] = 208;
	wa.waveform[6] = 161;
	wa.waveform[7] = 109;
	wa.waveform[8] = 60;
	wa.waveform[9] = 22;
	wa.waveform[10] = 2;
	wa.waveform[11] = 3;
	wa.waveform[12] = 24;
	wa.waveform[13] = 63;
	wa.waveform[14] = 128;
	wa.num_samples = 15;
	wa.sample_idx = 0;
#endif

  CpumonAct cpumon;
  cpumon_init(&cpumon);
  cpumon_main_loop();

  return 0;
}
