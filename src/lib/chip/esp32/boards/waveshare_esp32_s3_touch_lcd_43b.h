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

#include <string.h>

#include "esp_lcd_panel_rgb.h"

// Waveshare ESP32-S3-Touch-LCD-4.3B: an ESP32-S3-WROOM-1-N16R8 behind a
// 4.3" 800x480 RGB panel (ST7262 timing controller) with GT911 touch, a
// PCF85063A RTC, a CH422G I/O expander, RS485, CAN, a TF slot and two
// isolated inputs and outputs. The USB-C connector is the S3's native USB;
// there is no USB-UART bridge, and UART0's default pins drive the RS485
// transceiver.

#define WAVESHARE_43B_LCD_H_RES 800
#define WAVESHARE_43B_LCD_V_RES 480

// I2C bus shared by the touch controller, RTC and expander
#define WAVESHARE_43B_I2C_SDA   8
#define WAVESHARE_43B_I2C_SCL   9
#define WAVESHARE_43B_TOUCH_IRQ 4

// CAN (TWAI) transceiver
#define WAVESHARE_43B_CAN_TX 15
#define WAVESHARE_43B_CAN_RX 16

// TF card, SPI; its chip select is on the expander
#define WAVESHARE_43B_SD_MOSI 11
#define WAVESHARE_43B_SD_SCK  12
#define WAVESHARE_43B_SD_MISO 13

// CH422G expander outputs
#define WAVESHARE_43B_EXIO_TP_RST        1
#define WAVESHARE_43B_EXIO_LCD_BACKLIGHT 2
#define WAVESHARE_43B_EXIO_SD_CS         4

// The panel's RGB interface: 16-bit RGB565 on the S3's LCD peripheral.
// Timings are Waveshare's; the panel is a dumb timing controller and needs
// no initialization sequence.
static inline esp_lcd_rgb_panel_config_t waveshare_43b_rgb_panel_config(void) {
  esp_lcd_rgb_panel_config_t config;
  memset(&config, 0, sizeof(config));
  config.clk_src = LCD_CLK_SRC_DEFAULT;
  config.timings.pclk_hz = 16 * 1000 * 1000;
  config.timings.h_res = WAVESHARE_43B_LCD_H_RES;
  config.timings.v_res = WAVESHARE_43B_LCD_V_RES;
  config.timings.hsync_pulse_width = 4;
  config.timings.hsync_back_porch = 8;
  config.timings.hsync_front_porch = 8;
  config.timings.vsync_pulse_width = 4;
  config.timings.vsync_back_porch = 8;
  config.timings.vsync_front_porch = 8;
  config.timings.flags.pclk_active_neg = 1;
  config.data_width = 16;
  config.bits_per_pixel = 16;
  // two whole frames in PSRAM, so a frame is drawn off-screen and then
  // swapped in
  config.num_fbs = 2;
  config.flags.fb_in_psram = 1;
  // bounce buffers in internal RAM feed the panel through any PSRAM
  // bandwidth hiccup
  config.bounce_buffer_size_px = 10 * WAVESHARE_43B_LCD_H_RES;
  config.hsync_gpio_num = 46;
  config.vsync_gpio_num = 3;
  config.de_gpio_num = 5;
  config.pclk_gpio_num = 7;
  config.disp_gpio_num = -1;  // the backlight is on the expander
  // data[0..4] = B3..B7, data[5..10] = G2..G7, data[11..15] = R3..R7
  const int data_gpio_nums[16] = {14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40};
  memcpy(config.data_gpio_nums, data_gpio_nums, sizeof(data_gpio_nums));
  return config;
}
