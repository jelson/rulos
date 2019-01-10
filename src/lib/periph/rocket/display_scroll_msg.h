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

#ifndef __display_scroll_msg_h__
#define __display_scroll_msg_h__

#include "core/clock.h"
#include "periph/7seg_panel/board_buffer.h"

typedef struct s_dscrollmsgact {
  BoardBuffer bbuf;
  uint8_t len;
  uint8_t speed_ms;
  int index;
  const char *msg;
} DScrollMsgAct;

void dscrlmsg_init(struct s_dscrollmsgact *act, uint8_t board, const char *msg,
                   uint8_t speed_ms);

void dscrlmsg_set_msg(DScrollMsgAct *act, const char *msg);

#endif  // display_scroll_msg_h
