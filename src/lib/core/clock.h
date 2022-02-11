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

#include "core/hal.h"
#include "core/heap.h"
#include "core/time.h"

void init_clock(Time interval_us, uint8_t timer_id);

// Pretty cheap but only precise to one jiffy
Time clock_time_us();

// More expensive and more accurate
Time precise_clock_time_us();

// Synchronous delay that does not take interrupts
void delay_us(uint32_t delay);

void schedule_us(Time offset_us, ActivationFuncPtr func, void *data);
// schedule in the future. (asserts us>0)
void schedule_now(ActivationFuncPtr func, void *data);
// Be very careful with schedule_now -- it can result in an infinite
// loop if you schedule yourself for now repeatedly (because the clock
// never advances past now until the queue empties).
void schedule_absolute(Time at_time, ActivationFuncPtr func, void *data);

// LOG stats about the scheduler: the number of tasks scheduled, and their
// minimum and maximum periods.
void clock_log_stats();

#define Exp2Time(v) (((Time)1) << (v))
//#define schedule_ms(ms,act) { schedule_us(ms*1000, act); }

void scheduler_run();
