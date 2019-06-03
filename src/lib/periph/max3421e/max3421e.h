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

#include "max3421e-impl.h"
#include "usbstructs.h"

#include <stdint.h>

#include "core/rulos.h"

// Public interface to the max3421e library
bool max3421e_init(max3421e_t *max);
uint8_t max3421e_read_data(usb_device_t *dev, usb_endpoint_t *endpoint,
                           uint8_t *result_buf, uint16_t max_result_len,
                           uint16_t *result_len_received);
uint8_t max3421e_set_hid_idle(usb_device_t *dev, usb_endpoint_t *endpoint,
                              const uint8_t idle_rate);
