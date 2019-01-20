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
#include "core/util.h"

typedef struct {
  // bounds
  uint8_t expscale;  // all internal values scaled by 1<<expscale
  int32_t min;
  int32_t max;
  uint32_t maxSpeed;  // per 1024ms, absolute value of velocity

  // state
  int32_t base;
  uint32_t base_time;
  int32_t velocity;            // per 1024ms
  uint32_t last_impulse_time;  // clock_time
} DriftAnim;

void drift_anim_init(DriftAnim *da, uint8_t expscale, int32_t initValue,
                     int32_t min, int32_t max, uint32_t maxSpeed);

int32_t da_read(DriftAnim *da);
int32_t da_read_clip(DriftAnim *da, uint8_t scale, r_bool clip);
// clip==TRUE => if we hit a min/max limit, set velocity to 0
void da_random_impulse(DriftAnim *da);
void da_set_velocity(DriftAnim *da, int32_t velocity);
void da_set_velocity_scaled(DriftAnim *da, int32_t velocity, uint8_t downscale);
void da_bound_velocity(DriftAnim *da);
void da_set_value(DriftAnim *da, int32_t value);
void da_set_random_value(DriftAnim *da);
void _da_update_base(DriftAnim *da);
void _da_randomize_velocity(DriftAnim *da);

