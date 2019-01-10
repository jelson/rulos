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

#include "periph/rocket/drift_anim.h"
#include "periph/rocket/rocket.h"

void drift_anim_init(DriftAnim *da, uint8_t expscale, int32_t initValue,
                     int32_t min, int32_t max, uint32_t maxSpeed) {
  da->expscale = expscale;
  da->min = min << expscale;
  da->max = max << expscale;
  da->maxSpeed = maxSpeed << expscale;
  da_random_impulse(da);  // assign a valid velocity.
  da->base = initValue << expscale;
  da->base_time = clock_time_us();
}

int32_t _da_eval(DriftAnim *da, Time t, r_bool clip) {
  int32_t dt = (t - da->base_time) / 1000;
  int32_t nv = da->base + ((da->velocity * dt) >> 10);
  int32_t res = bound(nv, da->min, da->max);

  if (clip && res != nv) {
    _da_update_base(da);
    da->velocity = 0;
  }

  return res;
}

int32_t da_read(DriftAnim *da) { return da_read_clip(da, 0, FALSE); }

int32_t da_read_clip(DriftAnim *da, uint8_t scale, r_bool clip) {
  return _da_eval(da, clock_time_us(), clip) >> (da->expscale - scale);
}

void _da_update_base(DriftAnim *da) {
  Time t = clock_time_us();
  da->base = _da_eval(da, t, FALSE);
  da->base_time = t;
}

void da_random_impulse(DriftAnim *da) {
  _da_update_base(da);
  // select a new velocity
  da->velocity = (deadbeef_rand() % (da->maxSpeed * 2 + 1)) - da->maxSpeed;
}

void da_set_velocity(DriftAnim *da, int32_t velocity) {
  da_set_velocity_scaled(da, velocity, 0);
}

void da_set_velocity_scaled(DriftAnim *da, int32_t velocity,
                            uint8_t downscale) {
  _da_update_base(da);
  da->velocity = velocity << (da->expscale - downscale);
  da_bound_velocity(da);
}

void da_bound_velocity(DriftAnim *da) {
  da->velocity = bound(da->velocity, -da->maxSpeed, da->maxSpeed);
}

void da_set_value(DriftAnim *da, int32_t value) {
  da->base = value << da->expscale;
  da->base_time = clock_time_us();
}

void da_set_random_value(DriftAnim *da) {
  int32_t scaledrange = da->max - da->min;
  int32_t scaledvalue = (deadbeef_rand() % scaledrange) + da->min;
  da->base = scaledvalue;
  da->base_time = clock_time_us();
}
