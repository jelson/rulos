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

#include "periph/eeprom/eeprom.h"

#include <stdbool.h>

#if SIM
#define eeprom_write_word(x, y) \
  { (void)y; }
#define eeprom_write_block(x, y, z) \
  {                                 \
    (void)x;                        \
    (void)y;                        \
    (void)z;                        \
  }
#define eeprom_read_word(x) ((uint16_t)0)
#define eeprom_read_block(x, y, z) \
  {                                \
    (void)x;                       \
    (void)y;                       \
    (void)z;                       \
  }
#else
#include <avr/eeprom.h>
#endif  // SIM

// debug:
#define SYNCDEBUG() syncdebug(0, 'E', __LINE__)
void syncdebug(uint8_t spaces, char f, uint16_t line);

const char *EEPROM_BASE = (const char *)4;
const uint16_t EEPROM_MAGIC = (0xee10);

uint16_t _eeprom_checksum(uint8_t *buf, int len) {
  uint16_t v = 0;
  int i = 0;
  for (i = 0; i < len; i++) {
    v = v * 131 + buf[len];
  }
  return v;
}

void eeprom_write(uint8_t *buf, int len) {
  eeprom_write_word((uint16_t *)EEPROM_BASE, EEPROM_MAGIC);
  eeprom_write_block(buf, (void *)(EEPROM_BASE + 2), len);
  uint16_t checksum = _eeprom_checksum(buf, len);
  eeprom_write_word((uint16_t *)(EEPROM_BASE + 2 + len), checksum);
}

bool eeprom_read(uint8_t *buf, int len) {
  uint16_t magic = eeprom_read_word((uint16_t*)EEPROM_BASE);
  syncdebug(1, 'm', magic);
  if (magic != EEPROM_MAGIC) {
    SYNCDEBUG();
    return false;
  }

  eeprom_read_block(buf, (void *)(EEPROM_BASE + 2), len);

  uint16_t computed_checksum = _eeprom_checksum(buf, len);
  syncdebug(2, 'c', computed_checksum);
  uint16_t stored_checksum = eeprom_read_word((uint16_t *)(EEPROM_BASE + 2 + len));
  syncdebug(2, 's', stored_checksum);
  if (computed_checksum != stored_checksum) {
    SYNCDEBUG();
    return false;
  }

  SYNCDEBUG();
  return true;
}
