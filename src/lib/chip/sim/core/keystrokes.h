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

#pragma once

#include "core/util.h"

// Top-mode composable handler; returns false to fall through to next handler.
typedef bool (*sim_keystroke_handler)(char k);

// Special-mode handler that takes over all input until it exits.
typedef void (*sim_special_input_handler_t)(int c);
typedef void (*sim_input_handler_stop_t)();

// Register a handler to absorb keystrokes, such as keypad or ADC.
void sim_maybe_init_and_register_keystroke_handler(
    sim_keystroke_handler handler);

void sim_install_modal_handler(sim_special_input_handler_t handler,
                               sim_input_handler_stop_t stop);
