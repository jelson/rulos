/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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

#include <ctype.h>

#include "core/morse.h"
#include "core/rulos.h"

#ifdef SIM
#include <stdio.h>

#define debug(x...)       \
  do {                    \
    printf(x);            \
    printf("          "); \
    fflush(stdout);       \
  } while (0)
#else
#define debug(x...)
#endif

static const char* const morse_alphabet[] = {
    ".-",    // a
    "-...",  // b
    "-.-.",  // c
    "-..",   // d
    ".",     // e
    "..-.",  // f
    "--.",   // g
    "....",  // h
    "..",    // i
    ".---",  // j
    "-.-",   // k
    ".-..",  // l
    "--",    // m
    "-.",    // n
    "---",   // o
    ".--.",  // p
    "--.-",  // q
    ".-.",   // r
    "...",   // s
    "-",     // t
    "..-",   // u
    "...-"   // v
    ".--",   // w
    "-..-",  // x
    "-.--",  // y
    "--..",  // z
};

typedef struct {
  // inputs
  const char* send_string;
  uint32_t dot_time_us;
  MorseOutputToggleFunc* toggle_func;
  MorseOutputDoneFunc* done_func;

  // dynamically kept state
  const char* curr_letter;
  uint8_t is_keyed;
} morse_state_t;

morse_state_t morse_state;

static void morse_go(morse_state_t* morse_state);

static void morse_wait(morse_state_t* morse_state, uint8_t dot_times) {
  schedule_us(dot_times * morse_state->dot_time_us, (ActivationFuncPtr)morse_go,
              morse_state);
}

static void morse_go(morse_state_t* morse_state) {
  // If we're currently keyed (i.e., mid-symbol), unkey, and determine what
  // comes next.
  if (morse_state->is_keyed) {
    morse_state->toggle_func(0);
    morse_state->is_keyed = 0;

    // Move to the next symbol. If there are more symbols in this character,
    // wait the inter-symbol delay (1 dot time).
    morse_state->curr_letter++;
    if (*morse_state->curr_letter != '\0') {
      debug("inter-symbol wait");
      morse_wait(morse_state, 1);
      return;
    }

    // End of letter. Move to the next letter. If there are more letters in
    // the message, wait the inter-letter delay (3 dot times).
    morse_state->curr_letter = NULL;
    morse_state->send_string++;
    if (*morse_state->send_string != '\0') {
      debug("inter-letter wait");
      morse_wait(morse_state, 3);
      return;
    }

    // End of message. Call the done callback.
    debug("done");
    if (morse_state->done_func != NULL) {
      morse_state->done_func();
    }
    return;
  }

  assert(morse_state->send_string != NULL);

  // If we are starting a new letter, look it up.
  if (morse_state->curr_letter == NULL) {
    const char curr_letter = tolower(*morse_state->send_string);

    // If this is a space, wait the inter-word time (7 dot times) and advance
    // to the next character.
    if (curr_letter == ' ') {
      debug("inter-word wait");
      morse_state->send_string++;
      morse_wait(morse_state, 4);
      return;
    }

    assert(curr_letter >= 'a' && curr_letter <= 'z');
    debug("starting %c ", curr_letter);
    morse_state->curr_letter = morse_alphabet[curr_letter - 'a'];
  }

  // Key on!
  morse_state->toggle_func(1);
  morse_state->is_keyed = 1;

  // Wait to key off depending on if this is a dot or dash.
  const char curr_symbol = *morse_state->curr_letter;
  if (curr_symbol == '.') {
    debug(" keying dot");
    morse_wait(morse_state, 1);
  } else {
    debug(" keying dash");
    morse_wait(morse_state, 3);
  }
}

void emit_morse(const char* send_string, const uint32_t dot_time_us,
                MorseOutputToggleFunc* toggle_func,
                MorseOutputDoneFunc* done_func) {
  morse_state.send_string = send_string;
  morse_state.dot_time_us = dot_time_us;
  morse_state.toggle_func = toggle_func;
  morse_state.done_func = done_func;
  morse_state.is_keyed = 0;
  morse_state.curr_letter = NULL;
  schedule_now((ActivationFuncPtr)morse_go, &morse_state);
}
