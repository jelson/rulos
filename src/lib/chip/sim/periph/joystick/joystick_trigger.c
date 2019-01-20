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

#include "core/keystrokes.h"
#include "core/util.h"

// Joystick button here; motion in periph/adc.

r_bool sim_joystick_keystroke_handler(char c);

r_bool g_joystick_trigger_state;

void hal_init_joystick_button() {
  g_joystick_trigger_state = FALSE;
  sim_maybe_init_and_register_keystroke_handler(sim_joystick_keystroke_handler);
}

r_bool hal_read_joystick_button() { return g_joystick_trigger_state; }

r_bool sim_joystick_keystroke_handler(char c) {
  switch (c) {
    case '%':  // button-on
      g_joystick_trigger_state = !g_joystick_trigger_state;
      return true;
    default:
      return false;
  }
}
