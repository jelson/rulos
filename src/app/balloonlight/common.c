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

#include "common.h"

#include "core/hardware.h"
#include "core/rulos.h"
#include "stm32g0xx_hal_i2c.h"
#include "stm32g0xx_hal_rcc.h"
#include "stm32g0xx_ll_exti.h"
#include "stm32g0xx_ll_i2c.h"
#include "stm32g0xx_ll_pwr.h"

#define ACCEL_7BIT_ADDR 0b0010010

#define ACCEL_WRITE_ADDR (ACCEL_7BIT_ADDR << 1)
#define ACCEL_READ_ADDR  ((ACCEL_7BIT_ADDR << 1) | 1)

I2C_HandleTypeDef i2c_handle;

#define NUM_REGISTERS 0x3f
uint8_t chip_id = 0;
uint8_t orig_registers[NUM_REGISTERS];

// write to one of the accelerometer's registers
static void reg_write(uint8_t reg_addr, uint8_t value) {
  uint8_t buf[2];

  buf[0] = reg_addr;
  buf[1] = value;

  HAL_I2C_Master_Transmit(&i2c_handle, ACCEL_WRITE_ADDR, buf, sizeof(buf),
                          HAL_MAX_DELAY);
}

// read from one or more accelerometer registers, writing results to inbuf
static void reg_read(uint8_t reg_addr, void *inbuf, uint8_t len) {
  uint8_t outbuf[1];

  outbuf[0] = reg_addr;

  HAL_I2C_Master_Transmit(&i2c_handle, ACCEL_WRITE_ADDR, outbuf, sizeof(outbuf),
                          HAL_MAX_DELAY);
  HAL_I2C_Master_Receive(&i2c_handle, ACCEL_READ_ADDR, inbuf, len,
                         HAL_MAX_DELAY);
}

// convert one axis of acclerometer data to a 16-bit value. It's only 14 bits
// that are actually returned, but we pad it out to 16 bits to make the math a
// little easier.
void convert_accel_axis(uint8_t raw_low, uint8_t raw_high, int16_t *out) {
  *out = (raw_high << 8) | ((raw_low & 0b11111100) << 2);
}

// Read the acceleration in all 3 axes and write the signed 16-bit results to
// accel_out. Data registers start at register 0x1.
void read_accel_values(int16_t accel_out[3]) {
  uint8_t buf[6];
  reg_read(0x1, buf, sizeof(buf));

  for (int i = 0; i < 3; i++) {
    convert_accel_axis(buf[i * 2], buf[i * 2 + 1], &accel_out[i]);
  }
}

void start_accel() {
  gpio_make_output(LIGHT_EN);
  gpio_set(LIGHT_EN);
  gpio_make_input_disable_pullup(ACCEL_INT);

  // I2C pins are on PA11 (I2C2 SCL) and PA12 (I2C2 SDA)
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF6_I2C2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_12;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Enable the I2C peripheral
  __HAL_RCC_I2C2_CLK_ENABLE();
  i2c_handle.Instance = I2C2;
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

  // turn on accelerometer and slow down clock; power control register is 0x11.
  reg_write(0x11,
            (1 << 7)        // put accel in active mode
                | (0b0111)  // set MCLK to 5khz
  );

  // read chip id
  reg_read(0x0, &chip_id, 1);

  // configure ODR
  reg_write(0x10, 0b011);

  // set threshold for motion interrupt to be ... something. determined
  // experimentally.
  reg_write(0x2e, 50);

  // enable x, y and z axes for ANY_MOT interrupt
  reg_write(0x18, 0b111);

  // configure int1 to be ANY_MOT
  reg_write(0x1a, 0x62 | 1);

  // read all registers for debug purposes
  reg_read(0x0, &orig_registers, NUM_REGISTERS);
}
