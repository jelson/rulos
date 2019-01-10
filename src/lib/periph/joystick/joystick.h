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

#include "core/util.h"

#ifndef _BV
#define _BV(x) (1 << (x))
#endif

#define JOYSTICK_STATE_UP _BV(0)
#define JOYSTICK_STATE_DOWN _BV(1)
#define JOYSTICK_STATE_LEFT _BV(2)
#define JOYSTICK_STATE_RIGHT _BV(3)
#define JOYSTICK_STATE_TRIGGER _BV(4)
#define JOYSTICK_STATE_DISCONNECTED _BV(5)
typedef struct {
  // ADC channel numbers.  Should be initialized by caller before
  // joystick_init is called.
  uint8_t x_adc_channel;
  uint8_t y_adc_channel;

  // X and Y positions (from -100 to 100) of joystick, and a state
  // bitvector, valid after joystick_poll is called.
  int8_t x_pos;
  int8_t y_pos;
  uint8_t state;
} JoystickState_t;

void joystick_init(JoystickState_t *js);
void joystick_poll(JoystickState_t *js);
