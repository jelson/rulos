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

#include <inttypes.h>

typedef void (MorseOutputToggleFunc)(uint8_t onoff);
typedef void (MorseOutputDoneFunc)();

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
void emit_morse(const char* send_string,
		const uint32_t dot_time_us,
		MorseOutputToggleFunc* toggle_func,
		MorseOutputDoneFunc* done_func);
