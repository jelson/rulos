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

#include "periph/audio/audio_out.h"

#include "core/hal.h"

extern void syncdebug(uint8_t spaces, char f, uint16_t line);
#define SYNCDEBUG() syncdebug(0, 'O', __LINE__)

void _ao_handler(void *data);

void init_audio_out(AudioOut *ao, uint8_t timer_id, ActivationFuncPtr fill_func,
                    void *fill_data) {
  memset(ao->buffers, 255, sizeof(ao->buffers));
  int i;
  for (i = 0; i < AO_BUFLEN; i++) {
    ao->buffers[0][i] = i * 2;
    ao->buffers[1][AO_BUFLEN - i] = i * 2;
  }
  ao->sample_index = 0;
  ao->fill_buffer = 0;
  ao->play_buffer = 0;
  ao->fill_act.func = fill_func;
  ao->fill_act.data = fill_data;
  hal_audio_init();
#define SAMPLE_RATE 12000
  hal_start_clock_us(1000000 / SAMPLE_RATE, &_ao_handler, ao, timer_id);
  SYNCDEBUG();
}

void _ao_handler(void *data) {
  static uint8_t val = 0;
  val = 0x10 + val;
  hal_audio_fire_latch();
  AudioOut *ao = (AudioOut *)data;
  uint8_t sample = ao->buffers[ao->play_buffer][ao->sample_index];
  hal_audio_shift_sample(sample);
  ao->sample_index = (ao->sample_index + 1) & AO_BUFMASK;
  if ((ao->sample_index & AO_BUFMASK) == 0) {
    ao->fill_buffer = ao->play_buffer;
    ao->play_buffer += 1;
    if (ao->play_buffer >= AO_NUMBUFS) {
      ao->play_buffer = 0;
    }
    schedule_now(ao->fill_act.func, ao->fill_act.data);
  }
}
