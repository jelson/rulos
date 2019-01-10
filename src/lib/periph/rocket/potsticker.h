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

#ifndef _POTSTICKER_H
#define _POTSTICKER_H

#include "periph/input_controller/input_controller.h"
#include "periph/rocket/rocket.h"

typedef struct s_potsticker {
  uint8_t adc_channel;
  InputInjectorIfc *ifi;
  uint8_t detents;
  int8_t hysteresis;
  char fwd;
  char back;

  int8_t last_digital_value;
} PotSticker;

void init_potsticker(PotSticker *ps, uint8_t adc_channel, InputInjectorIfc *ifi,
                     uint8_t detents, char fwd, char back);

#endif  // _POTSTICKER_H
