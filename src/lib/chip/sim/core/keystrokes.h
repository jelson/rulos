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

// Top-mode composable handler; returns false to fall through to next handler.
typedef r_bool (*sim_keystroke_handler)(char k);

// Special-mode handler that takes over all input until it exits.
typedef void (*sim_special_input_handler_t)(int c);
typedef void (*sim_input_handler_stop_t)();

// Register a handler to absorb keystrokes, such as keypad or ADC.
void sim_maybe_init_and_register_keystroke_handler(
    sim_keystroke_handler handler);

void sim_install_modal_handler(sim_special_input_handler_t handler,
                               sim_input_handler_stop_t stop);
