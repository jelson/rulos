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

#include "periph/rocket/drift_anim.h"

#include "periph/rocket/rocket.h"

void drift_anim_init(DriftAnim *da, uint8_t expscale, int32_t initValue,
                     int32_t min, int32_t max, uint32_t maxSpeed) {
  da->expscale = expscale;
  da->min = min << expscale;
  da->max = max << expscale;
  da->maxSpeed = maxSpeed << expscale;
  _da_randomize_velocity(da);  // assign a valid velocity.
  da->base = initValue << expscale;
  da->base_time = clock_time_us();
}

int32_t _da_eval(DriftAnim *da, Time t, bool clip) {
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

int32_t da_read_clip(DriftAnim *da, uint8_t scale, bool clip) {
  return _da_eval(da, clock_time_us(), clip) >> (da->expscale - scale);
}

void _da_update_base(DriftAnim *da) {
  Time t = clock_time_us();
  da->base = _da_eval(da, t, FALSE);
  da->base_time = t;
}

void _da_randomize_velocity(DriftAnim *da) {
  da->velocity = (deadbeef_rand() % (da->maxSpeed * 2 + 1)) - da->maxSpeed;
}

void da_random_impulse(DriftAnim *da) {
  _da_update_base(da);
  _da_randomize_velocity(da);
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
