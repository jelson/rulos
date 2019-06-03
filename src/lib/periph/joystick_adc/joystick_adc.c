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

#include "periph/joystick_adc/joystick_adc.h"

#include "core/hal.h"

#define JOYSTICK_ADC_SCAN_RATE 10000

// Using 30k resistor:
//    X direction -- adc1, full left = 1020, center=470, right=305
//          left trigger   900 on/800 off
//          right trigger  320 on/350 off
//    Y direction -- adc0, full up   = 1020, center=470, down=300

// The joystick circuit looks like this:
//
// (Vref, Joy input)
//      |
// (Joy output, ADC input, resistor lead 1)
//      |
// (resistor lead 2, gnd)
//
// The joystick is in series with a fixed-size resistor (R).  The joystick
// resistance, which ranges from 0 to 100kohm, is X.  Therefore the total
// resistance is X+R, and at any given moment the portion at the resistor is
// R/X+R. Since the ADC is scaled from 0..1023, and R=39.2Kohm, then the ADC
// value will be
//      ADC = 1023 * (X/X+R).
// Solving for X gives us
//      X = [(39.2*1023) / ADC]  - 40.  (X in Kohm)
// That will give us X between 0 and 100, since the joystick's range is from
// 0-100Kohm.
//
// Next we want to map the resistance, which ranges from 0..100kohm, to
// -100 to 100: (cheating with Mathematica to help with the algebra)
//   100scale = (80204 / adc) - 178

static r_bool adc_to_100scale(uint16_t adc, int8_t *hscale_out) {
  // Values of < 100 correspond to resistances of over 300Kohm and
  // most likely are simply a disconnected joystick.
  if (adc < 100) {
    return FALSE;
  }

  int32_t retval = ((int32_t)80204 / adc) - 178;

  if (retval < -99) {
    *hscale_out = -99;
  } else if (retval > 99) {
    *hscale_out = 99;
  } else {
    *hscale_out = retval;
  }
  return TRUE;
}

static bool joystick_adc_read(JoystickState_t *js_base) {
  Joystick_ADC_t *js = (Joystick_ADC_t *)js_base;

  if (!adc_to_100scale(hal_read_adc(js->x_adc_channel), &js->base.x_pos)) {
    return FALSE;
  }

  if (!adc_to_100scale(hal_read_adc(js->y_adc_channel), &js->base.y_pos)) {
    return FALSE;
  }

  if (hal_read_joystick_button()) {
    js->base.state |= JOYSTICK_STATE_TRIGGER;
  } else {
    js->base.state &= ~(JOYSTICK_STATE_TRIGGER);
  }

  return TRUE;
}

void init_joystick_adc(Joystick_ADC_t *js, uint8_t x_chan, uint8_t y_chan) {
  js->base.state = 0;
  js->base.joystick_reader_func = joystick_adc_read;

  js->x_adc_channel = x_chan;
  js->y_adc_channel = y_chan;

  hal_init_adc(JOYSTICK_ADC_SCAN_RATE);
  hal_init_adc_channel(js->x_adc_channel);
  hal_init_adc_channel(js->y_adc_channel);
  hal_init_joystick_button();
}
