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

#include "core/time.h"
#include "flash_dumper.h"
#include "periph/uart/linereader.h"
#include "periph/uart/uart.h"

typedef struct serial_reader_t_s serial_reader_t;

typedef void (*serial_reader_cb_t)(serial_reader_t *sr, const char *line);

struct serial_reader_t_s {
  UartState_t uart;
  LineReader_t linereader;
  flash_dumper_t *flash_dumper;
  int num_total_lines;
  Time last_active;
  serial_reader_cb_t cb;
};

// initialize a serial reader -- reads from a uart and pipes the output to a
// flash dumper
void serial_reader_init(serial_reader_t *sr, uint8_t uart_id, uint32_t baud,
                        flash_dumper_t *flash_dumper, serial_reader_cb_t cb);

// manually append a line to a serial reader
void serial_reader_print(serial_reader_t *sr, const char *s);

// returns true if the serial reader has received data recently
bool serial_reader_is_active(serial_reader_t *sr);
