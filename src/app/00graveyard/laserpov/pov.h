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

#include "mirror.h"
#include "periph/rocket/rocket.h"

#define POV_BITMAP_LENGTH (256)

typedef struct {
  ActivationFunc func;
  MirrorHandler *mirror;
  uint8_t laser_board;
  uint8_t laser_digit;
  SSBitmap bitmap[POV_BITMAP_LENGTH];
} PovHandler;

void pov_init(PovHandler *pov, MirrorHandler *mirror, uint8_t laser_board,
              uint8_t laser_digit);
