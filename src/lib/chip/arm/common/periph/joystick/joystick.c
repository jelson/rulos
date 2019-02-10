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

#include "core/board_defs.h"
#include "core/hal.h"
#include "core/hardware.h"

void hal_init_joystick_button() {
  gpio_make_input_enable_pullup(JOYSTICK_TRIGGER);
}

r_bool hal_read_joystick_button() { return gpio_is_clr(JOYSTICK_TRIGGER); }
