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

/*
 * This is a firmware for a simple motion-activated coin-cell light based on the
 * STM32G031 CPU and the QMA7981 accelerometer. I picked the STM32G031 because I
 * like the STM32 line, the G0 is a fantastic new series that's incredibly
 * capable but cheap, and ST recently introced a tiny and incredibly low-cost
 * 8-pin version (the STM32G031J6M6). LCSC sells it for just 55 cents.
 *
 * I picked the QMA7981 accelerometer for no particular reason other than that
 * it's the cheapest acclerometer on LCSC. It has low quiescent power when
 * sampling at low rate and can generate interrupts on motion.
 *
 * The program doesn't do much: it configures the accelerometer to generate
 * interrupts when motion is detected on any of the three axes, then turns on
 * the light. The light is controlled by a combination boost-converter LED
 * driver, the MT9284, which can boost the voltage of the coin cell enough to
 * light up to 6 white LEDs in series. It has an enable pin.
 *
 * The goal is to keep idle power to just a few microamps so that the device can
 * last for a long time (years) on a single coin cell. The STM32 has low-power
 * modes, e.g. STANDBY mode in which the device can be waken by an external
 * interrupt on a special pin (WKUP) and only uses 300nA if ULPEN=1.
 */

#include "core/hardware.h"
#include "core/rulos.h"

#include "stm32g0xx_hal_i2c.h"
#include "stm32g0xx_hal_rcc.h"
#include "stm32g0xx_ll_exti.h"
#include "stm32g0xx_ll_i2c.h"
#include "stm32g0xx_ll_pwr.h"

////// configuration //////

#define LIGHT_EN GPIO_B7
#define ACCEL_INT GPIO_B5

#define ACCEL_7BIT_ADDR 0b0010010
#define SAMPLING_TIME_MSEC 10
#define LIGHT_TIMEOUT_SEC 5
#define NUM_REGISTERS 0x3f

///////////////////////////

#define ACCEL_WRITE_ADDR (ACCEL_7BIT_ADDR << 1)
#define ACCEL_READ_ADDR ((ACCEL_7BIT_ADDR << 1) | 1)

I2C_HandleTypeDef i2c_handle;

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
static void read_accel_values(int16_t accel_out[3]) {
  uint8_t buf[6];
  reg_read(0x1, buf, sizeof(buf));

  for (int i = 0; i < 3; i++) {
    convert_accel_axis(buf[i * 2], buf[i * 2 + 1], &accel_out[i]);
  }
}

void start_accel() {
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
      __LL_I2C_CONVERT_TIMINGS(1,  // Prescaler
                               13, // SCLDEL, data setup time
                               13, // SDADEL, data hold time
                               54, // SCLH, clock high period
                               84  // SCLL, clock low period
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

  // turn on accelerometer. Power control register is 0x11.
  reg_write(0x11,
            //(1 << 7) | // put accel in active mode
            0x40); // default values (from docs) for all other bits

  // read all registers for debug purposes
  reg_read(0x0, &orig_registers, NUM_REGISTERS);

  // read chip id
  reg_read(0x0, &chip_id, 1);

  // set threshold for motion interrupt to be ... something. determined
  // experimentally.
  reg_write(0x2e, 50);

  // enable x, y and z axes for ANY_MOT interrupt
  reg_write(0x18, 0b111);

  // configure int1 to be ANY_MOT
  reg_write(0x1a, 0x62 | 1);
}

void turn_on_led_when_upside_down() {
  static int16_t last_accel[3] = {};
  static int16_t ewma_accel[3] = {};
  static bool led_is_on = false;

  read_accel_values(last_accel);

  for (int i = 0; i < 3; i++) {
    ewma_accel[i] = (last_accel[i] / 4) + (3 * (ewma_accel[i] / 4));
  }

  if (led_is_on) {
    if (ewma_accel[2] > 100) {
      led_is_on = false;
    }
  } else {
    if (ewma_accel[2] < -100) {
      led_is_on = true;
    }
  }

  gpio_set_or_clr(LIGHT_EN, led_is_on);
}

void turn_on_led_when_jostled() {
  static uint32_t led_on_time_ms = 0;

  if (gpio_is_set(ACCEL_INT)) {
    // motion
    led_on_time_ms = LIGHT_TIMEOUT_SEC * 1000;
  } else {
    // no motion
    if (led_on_time_ms > 0) {
      led_on_time_ms -= SAMPLING_TIME_MSEC;
    }
  }

  gpio_set_or_clr(LIGHT_EN, led_on_time_ms > 0);
}

void toggle_led() {
  static int led_counter = 0;

  led_counter++;
  if (led_counter == 4) {
    led_counter = 0;
  }

  gpio_set_or_clr(LIGHT_EN, led_counter == 0);
}

void run_accel(void *data) {
  // This goes here instead of in main() because we want to give the
  // accelerometer sufficient turn-on time.
  if (data != NULL) {
    start_accel();
  }

#if 0
  HAL_SuspendTick();
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
  HAL_ResumeTick();
#endif

  // Set the pulldown on the LED driver control line
  HAL_PWREx_EnablePullUpPullDownConfig();
  HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_B, PWR_GPIO_BIT_7);

  volatile int j = 0;
  for (uint32_t i = 0; i < 1000000; i++) {
    j++;
  }

  // Go into super-low-power shutdown mode
  HAL_PWREx_EnterSHUTDOWNMode();
  // HAL_PWR_EnterSTANDBYMode();
  return;

  schedule_us(SAMPLING_TIME_MSEC * 1000, (ActivationFuncPtr)run_accel, NULL);

  // for testing:
  turn_on_led_when_upside_down();

  // for testing:
  // toggle_led();

  // real:
  // turn_on_led_when_jostled();
}

int main() {
  hal_init();

  init_clock(100000, TIMER1);
  gpio_make_output(LIGHT_EN);
  gpio_clr(LIGHT_EN);
  gpio_make_input_disable_pullup(ACCEL_INT);

  __HAL_RCC_PWR_CLK_ENABLE();

  // note: must be under 2mhz for this mode to work. See makefile for defines
  // that work in conjunction with stm32-hardware.c to set clock options to
  // 2mhz using HSI directly, disabling PLL.
  LL_PWR_EnableLowPowerRunMode();

  // The datasheet of the QMA7981 says the power-on time is 50 msec. We'll give
  // it 75 just to be safe.
  schedule_us(75000, (ActivationFuncPtr)run_accel, (void *)1);

  cpumon_main_loop();
}
