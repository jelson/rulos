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

#pragma once

#include "core/util.h"
#include "core/time.h"

typedef struct {
  r_bool is_pressed;
  Time refrac_time_us;
  Time next_valid_push_time;
} DebouncedButton_t;
  
void debounce_button_init(DebouncedButton_t* b, Time refrac_time_us);

// Pass in whether the raw (un-debounced) button is in the pushed state.
// Returns true if the debounced button was just pushed.
r_bool debounce_button(DebouncedButton_t* b, r_bool raw_is_down);
