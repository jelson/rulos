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

#ifndef _POV_H
#define _POV_H

#include "periph/rocket/rocket.h"

#ifndef SIM
#include "hardware.h"
#endif  // SIM

#define POV_LG_DISPLAY_WIDTH 5
#define POV_DISPLAY_WIDTH (1 << POV_LG_DISPLAY_WIDTH)

#define POVLEDA GPIO_B4
#define POVLEDB GPIO_B5
#define POVLEDC GPIO_B3
#define POVLEDD GPIO_D6
#define POVLEDE GPIO_B0

typedef struct {
  // written by measurement func; read by display func
  r_bool last_wave_positive;
  Time lastPhase;
  Time curPhase;
  Time lastPeriod;
  uint16_t debug_position;
  char debug_display_on;
  char debug_reverse;

  r_bool visible;

  uint8_t message[POV_DISPLAY_WIDTH];
} PovAct;

void pov_init(PovAct *povAct);
void pov_measure(PovAct *povAct);
void pov_display(PovAct *povAct);
void pov_write(PovAct *povAct, char *msg);
void pov_set_visible(PovAct *povAct, r_bool visible);

void pov_paint(uint8_t bitmap);

#endif  // _POV_H
