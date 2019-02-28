/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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

#include "periph/rocket/autotype.h"

void update_autotype(Autotype *a);

void init_autotype(Autotype *a, InputInjectorIfc *iii, char *str, Time period) {
  a->iii = iii;
  a->str = str;
  a->period = period;
  a->ptr = a->str;
  schedule_us(a->period, (ActivationFuncPtr)update_autotype, a);
}

void update_autotype(Autotype *a) {
  char c = *(a->ptr);
  if (c != '\0') {
    a->ptr += 1;
    LOG("Autotype(%c)", c);
    a->iii->func(a->iii, KeystrokeCtor(c));
    schedule_us(a->period, (ActivationFuncPtr)update_autotype, a);
  }
}
