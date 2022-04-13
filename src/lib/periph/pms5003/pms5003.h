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

#include <stdint.h>
#include <stdlib.h>

#include "core/queue.h"
#include "periph/uart/uart.h"

typedef struct {
  uint16_t pm10_standard;
  uint16_t pm25_standard;
  uint16_t pm100_standard;
  uint16_t pm10_env;
  uint16_t pm25_env;
  uint16_t pm100_env;
  uint16_t particles_03um;
  uint16_t particles_05um;
  uint16_t particles_10um;
  uint16_t particles_25um;
  uint16_t particles_50um;
  uint16_t particles_100um;
} pms5003_data_t;

typedef void (*pms5003_cb_t)(pms5003_data_t *data, void *user_data);

#define QLEN 200

typedef struct {
  UartState_t uart;
  pms5003_cb_t cb;
  void *user_data;
  union {
    char storage[sizeof(CharQueue) + QLEN];
    CharQueue q;
  } inq;
} pms5003_t;

void pms5003_init(pms5003_t *pms5003, uint8_t uart_number, pms5003_cb_t cb,
                  void *user_data);
