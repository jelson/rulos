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

#include "periph/7seg_panel/board_config.h"
#include "periph/7seg_panel/board_buffer.h"
#include "periph/7seg_panel/cursor.h"
#include "periph/7seg_panel/display_controller.h"
#include "periph/7seg_panel/region.h"
#include "periph/7seg_panel/remote_bbuf.h"

void hal_init_rocketpanel();
