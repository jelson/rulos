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

#include "core/heap.h"
#include "periph/uart/uart.h"

typedef struct {
  UartState_t uart;
  char line[40];
  char *line_ptr;
  ActivationRecord line_act;
} SerialConsole;

void serial_console_init(SerialConsole *sca, ActivationFuncPtr line_func,
                         void *line_data);
void serial_console_sync_send(SerialConsole *act, const char *buf,
                              uint16_t buflen);
