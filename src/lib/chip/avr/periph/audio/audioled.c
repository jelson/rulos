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

#include "core/hardware.h"
#include "core/rulos.h"

#define AUDIO_LED_RED GPIO_D2
#define AUDIO_LED_YELLOW GPIO_D3

void audioled_init() {
  gpio_make_output(AUDIO_LED_RED);
  gpio_make_output(AUDIO_LED_YELLOW);
}

void audioled_set(r_bool red, r_bool yellow) {
  gpio_set_or_clr(AUDIO_LED_RED, !red);
  gpio_set_or_clr(AUDIO_LED_YELLOW, !yellow);
}
