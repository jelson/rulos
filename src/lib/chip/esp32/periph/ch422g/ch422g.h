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

#include "driver/i2c_master.h"

// WCH CH422G I2C I/O expander: eight push-pull outputs EXIO0-7 and four
// open-drain outputs OD0-3. Each of its registers answers at its own 7-bit
// I2C address and takes a single data byte, so the chip appears on the bus
// as several devices.
typedef struct {
  i2c_master_dev_handle_t wr_set;
  i2c_master_dev_handle_t wr_io;
  i2c_master_dev_handle_t wr_oc;
  uint8_t exio;  // EXIO0-7 output levels
  uint8_t od;    // OD0-3 output levels
} ch422g_t;

// Attaches to the bus and configures EXIO0-7 as outputs, with every output
// released high.
void ch422g_init(ch422g_t *dev, i2c_master_bus_handle_t bus);

void ch422g_set_exio(ch422g_t *dev, uint8_t pin, bool level);
void ch422g_set_od(ch422g_t *dev, uint8_t pin, bool level);
