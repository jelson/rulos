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

#include <stdint.h>

class Time;

class Interval {
 public:
 Interval() : usec_(0) { }
  
  static Interval seconds(const uint32_t seconds) {
    return Interval(seconds * 1000000);
  }

  static Interval msec(const uint32_t msec) {
    return Interval(msec * 1000);
  }
  
  static Interval usec(const uint32_t usec) {
    return Interval(usec);
  }

  const uint32_t to_usec() const {
    return usec_;
  }

  const Interval operator=(const Interval& interval) {
    return Interval(interval.usec_);
  }


 private:
 Interval(const uint32_t usec) : usec_(usec) { }
  
  const uint32_t usec_;
};

// Definition of time for RulOS, in units of usec.
class Time {
 public:
  Time() {
    usec = 0;
  }
    
  bool later_than(const Time& other) {
    // the subtraction will roll over too
    return other.usec - usec > 0;

    // this took forever to puzzle out and was originally a complicated set of
    // conditionals
  }

  bool later_than_or_eq(const Time& other) {
    return other.usec - usec >= 0;
  }

  void operator+=(const Interval& interval) {
    usec += interval.to_usec();
  }

  volatile int32_t usec;
};

