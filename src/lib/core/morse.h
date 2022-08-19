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

#include <stdbool.h>
#include <stdint.h>

typedef void(MorseOutputToggleFunc)(bool onoff);
typedef void(MorseOutputDoneFunc)();

// Send a message in morse. send_string is the string to send; it may
// only contain alphabetic characters and spaces.
//
// dot_time_us is the dot time in microseconds; all times are scaled up from
// this as described in https://en.wikipedia.org/wiki/Morse_code#Timing
//
// toggle_func is a callback that turns your key on or off -- your app
// can connect it to an LED, buzzer, etc.
//
// done_func is a callback that will be called when the sending is
// done.  Assumes the RulOS scheduler is running, i.e. schedule_us()
// works.
void emit_morse(const char* send_string, const uint32_t dot_time_us,
                MorseOutputToggleFunc* toggle_func,
                MorseOutputDoneFunc* done_func);
