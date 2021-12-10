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

#define VOLT_PRESCALE_DIV1 (0b00 << 11)
#define VOLT_PRESCALE_DIV2 (0b01 << 11)
#define VOLT_PRESCALE_DIV4 (0b10 << 11)
#define VOLT_PRESCALE_DIV8 (0b11 << 11)

bool ina219_init(uint8_t device_addr, uint32_t prescale, uint16_t calibration);
bool ina219_read_microamps(uint8_t device_addr, int16_t *val /* OUT */);
