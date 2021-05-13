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

#include <stdint.h>

#include "core/rulos.h"

void hal_init_spi();
void hal_spi_set_fast(r_bool fast);
void hal_spi_select_slave(r_bool select);
void hal_spi_set_handler(HALSPIHandler *handler);

void hal_spi_send(uint8_t data);
void hal_spi_send_multi(uint8_t *buf_out, uint16_t len);
void hal_spi_sendrecv_multi(uint8_t *buf_out, uint8_t *buf_in, uint16_t len);
