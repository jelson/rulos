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

#pragma once

#include "periph/display_rtc/display_rtc.h"
#include "periph/input_controller/focus.h"
#include "periph/rocket/display_scroll_msg.h"

typedef struct {
  UIEventHandlerFunc func;
  DScrollMsgAct msgAct;
  DRTCAct rtcAct;
  BoardBuffer *bufs[2];
} LabeledDisplayHandler;

void labeled_display_init(LabeledDisplayHandler *ldh, int b0,
                          FocusManager *focus);
