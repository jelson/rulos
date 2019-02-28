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

#pragma once

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
