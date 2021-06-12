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

#include <stdbool.h>
#include <stdint.h>

#include "core/hardware.h"
#include "core/rulos.h"
#include "stm32g0xx_hal_i2c.h"
#include "stm32g0xx_hal_rcc.h"
#include "stm32g0xx_ll_exti.h"
#include "stm32g0xx_ll_i2c.h"
#include "stm32g0xx_ll_pwr.h"

#define WRITE_ADDR(addr) (addr << 1)
#define READ_ADDR(addr)  ((addr << 1) | 1)

I2C_HandleTypeDef i2c_handle;

// write to one of the 16-bit registers
static void reg_write(uint8_t device_addr, uint8_t reg_addr, uint16_t value) {
  uint8_t buf[3];

  buf[0] = reg_addr;
  buf[1] = value >> 8;
  buf[2] = value & 0xff;

  HAL_I2C_Master_Transmit(&i2c_handle, WRITE_ADDR(device_addr), buf,
                          sizeof(buf), HAL_MAX_DELAY);
}

// read from one of the 16-bit registers, writing results to inbuf
static void reg_read(uint8_t device_addr, uint8_t reg_addr,
                     uint16_t *value /* OUT */) {
  uint8_t outbuf[1];

  outbuf[0] = reg_addr;

  HAL_I2C_Master_Transmit(&i2c_handle, WRITE_ADDR(device_addr), outbuf,
                          sizeof(outbuf), HAL_MAX_DELAY);

  uint8_t inbuf[2];
  HAL_I2C_Master_Receive(&i2c_handle, READ_ADDR(device_addr), inbuf, 2,
                         HAL_MAX_DELAY);
  *value = inbuf[0] << 8 | inbuf[1];
}

static void start_i2c() {
  // I2C pins are on PB6 (I2C1 SCL) and PB9 (I2C1 SDA)
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF6_I2C1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_9;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // Enable the I2C peripheral
  __HAL_RCC_I2C1_CLK_ENABLE();
  i2c_handle.Instance = I2C1;
  i2c_handle.Init.Timing =
      __LL_I2C_CONVERT_TIMINGS(1,   // Prescaler
                               13,  // SCLDEL, data setup time
                               13,  // SDADEL, data hold time
                               54,  // SCLH, clock high period
                               84   // SCLL, clock low period
      );
  i2c_handle.Init.OwnAddress1 = 0;
  i2c_handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  i2c_handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  i2c_handle.Init.OwnAddress2 = 0;
  i2c_handle.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  i2c_handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  i2c_handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&i2c_handle) != HAL_OK) {
    __builtin_trap();
  }
}

void ina219_init(uint8_t device_addr) {
  static bool i2c_started = false;
  if (!i2c_started) {
    i2c_started = true;
    start_i2c();
  }

  // reset device
  uint16_t reset_reg = 1 << 15;
  reg_write(device_addr, 0x0, reset_reg);
  reg_read(device_addr, 0x0, &reset_reg);
  assert(reset_reg == 0x399f);

  // configuration register:
  // * no PG scaling
  // * 128 sample averaging
  // * bus voltage monitoring off, power measurement on
  uint16_t config_reg = 0b0001111111111101;
  reg_write(device_addr, 0x0, config_reg);

  // read back the config register and make sure it was set as we requested
  reg_read(device_addr, 0x0, &reset_reg);
  LOG("INA219@0x%x: configuration set to 0x%x", device_addr, reset_reg);
  assert(reset_reg == config_reg);

  // docs say calibration register should be trunc[0.04096 / (current_lsb *
  // R_shunt)] R_shunt is 5.1 ohms, we'll set current_lsb to be 1 microamp; that
  // gives us 8031. manually calibrated using 34401A to get a better value.
  int16_t cal = 8577;
  reg_write(device_addr, 0x5, cal);
}

bool ina219_read_microamps(uint8_t device_addr, int32_t *val /* OUT */) {
  uint16_t reg;

  // Read the bus voltage register, which also has the "conversion
  // ready" bit
  reg_read(device_addr, 0x2, &reg);

  if ((reg & (1 << 1)) == 0) {
    // conversion not ready!
    return false;
  }

#if 0
  int16_t voltage_register;
  reg_read(device_addr, 0x1, (uint16_t *) &voltage_register);
  LOG("voltage register: %d", voltage_register);
#endif

  reg_read(device_addr, 0x4, (uint16_t *)val);
  return true;
}
