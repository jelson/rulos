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

#include "core/time.h"

// Jiffy clock driven by an underlying hardware timer.
//
// One might imagine having clocks be regular objects where you could
// instantiate however many you want. However, we want to stick with the model
// of there being a global clock that can be queried for its time, which is
// a pretty typical operating system interface.
class Clock {
 public:
  Clock();
  
  static void Start(const Interval& jiffy_interval, uint8_t timer_id);

  // Pretty cheap but only precise to one jiffy. This should be used by most
  // functions.
  static Time GetTime();

  // More expensive but more precise, down to 1/1000th of a jiffy.
  static Time GetPreciseTime();

 private:
  // Handler called by the hardware timer.
  static void handler(void *data);

  // Jiffy clock updated by hardware timer callbacks.
  Time interrupt_driven_jiffy_clock_;

  // Interval between hardware timer interrupts. May not be exactly what was
  // requested when the clock was initialized, subject to rounding that might
  // happen by the hardware (e.g. due to limited prescaler selection).
  Interval jiffy_interval_;
};

