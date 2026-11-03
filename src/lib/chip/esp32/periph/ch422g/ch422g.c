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
#include "periph/ch422g/ch422g.h"

#include <string.h>

#include "core/rulos.h"

// register "addresses"
#define CH422G_WR_SET 0x24  // system parameters
#define CH422G_WR_OC  0x23  // OD0-3 output levels
#define CH422G_WR_IO  0x38  // EXIO0-7 output levels

// WR_SET bits
#define CH422G_WR_SET_IO_OE 0x01  // EXIO0-7 are outputs

#define CH422G_I2C_SPEED_HZ   400000
#define CH422G_I2C_TIMEOUT_MS 100

static i2c_master_dev_handle_t add_register(i2c_master_bus_handle_t bus, uint16_t address) {
  i2c_device_config_t config;
  memset(&config, 0, sizeof(config));
  config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  config.device_address = address;
  config.scl_speed_hz = CH422G_I2C_SPEED_HZ;
  i2c_master_dev_handle_t handle;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &config, &handle));
  return handle;
}

static void write_register(i2c_master_dev_handle_t reg, uint8_t value) {
  ESP_ERROR_CHECK(i2c_master_transmit(reg, &value, 1, CH422G_I2C_TIMEOUT_MS));
}

void ch422g_init(ch422g_t *dev, i2c_master_bus_handle_t bus) {
  dev->wr_set = add_register(bus, CH422G_WR_SET);
  dev->wr_io = add_register(bus, CH422G_WR_IO);
  dev->wr_oc = add_register(bus, CH422G_WR_OC);

  // set the output levels before enabling the drivers
  dev->exio = 0xFF;
  dev->od = 0x0F;
  write_register(dev->wr_io, dev->exio);
  write_register(dev->wr_oc, dev->od);
  write_register(dev->wr_set, CH422G_WR_SET_IO_OE);
}

static uint8_t with_bit(uint8_t value, uint8_t pin, bool level) {
  return level ? (value | (1 << pin)) : (value & ~(1 << pin));
}

void ch422g_set_exio(ch422g_t *dev, uint8_t pin, bool level) {
  assert(pin < 8);
  dev->exio = with_bit(dev->exio, pin, level);
  write_register(dev->wr_io, dev->exio);
}

void ch422g_set_od(ch422g_t *dev, uint8_t pin, bool level) {
  assert(pin < 4);
  dev->od = with_bit(dev->od, pin, level);
  write_register(dev->wr_oc, dev->od);
}
