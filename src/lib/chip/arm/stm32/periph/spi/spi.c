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

/* Definition for SPIx Pins */
#define SPIx_SCK_PIN GPIO_PIN_5
#define SPIx_SCK_GPIO_PORT GPIOA
#define SPIx_SCK_AF GPIO_AF5_SPI1
#define SPIx_MISO_PIN GPIO_PIN_6
#define SPIx_MISO_GPIO_PORT GPIOA
#define SPIx_MISO_AF GPIO_AF5_SPI1
#define SPIx_MOSI_PIN GPIO_PIN_7
#define SPIx_MOSI_GPIO_PORT GPIOA
#define SPIx_MOSI_AF GPIO_AF5_SPI1
#define GPIO_CHIPSELECT GPIO_A4

#include "periph/spi/hal_spi.h"

#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_hal_rcc_ex.h"
#include "stm32f3xx_hal_spi.h"

SPI_HandleTypeDef hspi;

void hal_init_spi(void) {
  // Configure GPIO: chip select
  gpio_make_output(GPIO_CHIPSELECT);
  gpio_set(GPIO_CHIPSELECT);

  // Configure GPIO: SCK
  GPIO_InitTypeDef gpio;
  memset(&gpio, 0, sizeof(gpio));
  gpio.Pin = SPIx_SCK_PIN;
  gpio.Alternate = SPIx_SCK_AF;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SPIx_SCK_GPIO_PORT, &gpio);

  // Configure GPIO: MISO
  gpio.Pin = SPIx_MISO_PIN;
  gpio.Alternate = SPIx_MISO_AF;
  HAL_GPIO_Init(SPIx_MISO_GPIO_PORT, &gpio);

  // Configure GPIO: MOSI
  gpio.Pin = SPIx_MOSI_PIN;
  gpio.Alternate = SPIx_MOSI_AF;
  HAL_GPIO_Init(SPIx_MOSI_GPIO_PORT, &gpio);

  __HAL_RCC_SPI1_CLK_ENABLE();

  memset(&hspi, 0, sizeof(hspi));
  hspi.Instance = SPI1;
  hspi.Init.Mode = SPI_MODE_MASTER;
  hspi.Init.Direction = SPI_DIRECTION_2LINES;
  hspi.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi.Init.NSS = SPI_NSS_HARD_OUTPUT;
  hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
  hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi.Init.CRCPolynomial = 10;
  HAL_SPI_Init(&hspi);
  __HAL_SPI_ENABLE(&hspi);
}

void hal_spi_select_slave(r_bool select) {
  gpio_set_or_clr(GPIO_CHIPSELECT, !select);
}

void hal_spi_send(uint8_t byte) { hal_spi_send_multi(&byte, 1); }

void hal_spi_send_multi(uint8_t *buf_out, const uint16_t len) {
  HAL_SPI_Transmit(&hspi, buf_out, len, 1000);
}

void hal_spi_sendrecv_multi(uint8_t *buf_out, uint8_t *buf_in,
                            const uint16_t len) {
  HAL_SPI_TransmitReceive(&hspi, buf_out, buf_in, len, 1000);
}
