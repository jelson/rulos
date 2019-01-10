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
#include "periph/7seg_panel/cursor.h"
#include "periph/7seg_panel/region.h"
#include "periph/input_controller/focus.h"

typedef struct {
  UIEventHandlerFunc func;
  const char **msgs;
  uint8_t len;
  uint8_t selected;
  RowRegion region;
  CursorAct cursor;
  UIEventHandler *notify;
} Knob;

void knob_init(Knob *knob, RowRegion region, const char **msgs, uint8_t len,
               UIEventHandler *notify, FocusManager *fa, const char *label);

void knob_set_value(Knob *knob, uint8_t value);
