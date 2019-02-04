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

#include "core/debounce.h"
#include "core/clock.h"
#include "core/util.h"

void debounce_button_init(DebouncedButton_t* b, Time refrac_time_us) {
  b->is_pressed = FALSE;
  b->refrac_time_us = refrac_time_us;
  b->next_valid_push_time = clock_time_us();
}

r_bool debounce_button(DebouncedButton_t* b, r_bool raw_is_down) {
  Time now = clock_time_us();

  // If we're already past the allowed-key-time, advance it to the present.
  // Otherwise, if half a clock-rollver period goes by with no keypress, a
  // keypress might be improperly ignored.
  if (later_than(now, b->next_valid_push_time)) {
    b->next_valid_push_time = now;
  }

  // If the button is in the same state as before, do nothing.
  if (raw_is_down == b->is_pressed) {
    return FALSE;
  }

  b->is_pressed = raw_is_down;

  if (raw_is_down) {
    // Button was just pressed. If it's inside the refractory window (taking
    // rollover into account), ignore it as a bounce. Otherwise, return a valid
    // button press.
    if (later_than_or_eq(now, b->next_valid_push_time)) {
      return TRUE;
    } else {
      return FALSE;
    }
  } else {
    // Button was just released. Set next valid press time to be after the
    // refractory time.
    b->next_valid_push_time = now + b->refrac_time_us;
    return FALSE;
  }
}
