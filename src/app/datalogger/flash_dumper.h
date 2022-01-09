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

#include <stdbool.h>
#include <stdint.h>

#include "core/wallclock.h"
#include "periph/fatfs/ff.h"
#include "periph/uart/linereader.h"

#define WRITE_INCREMENT 2048

typedef struct {
  FATFS fatfs;  // SD card filesystem global state
  FIL fp;
  char buf[WRITE_INCREMENT * 2];
  int len;
  bool ok;
  wallclock_t wallclock;
} flash_dumper_t;

void flash_dumper_init(flash_dumper_t *fd);
void flash_dumper_write(flash_dumper_t *fd, const void *buf, int len);
void flash_dumper_print(flash_dumper_t *fd, const char *s);
