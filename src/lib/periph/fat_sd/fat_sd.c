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

#include "periph/fat_sd/fat_sd.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/fat_sd/mem-ffs.h"
#include "periph/fat_sd/mem-mmcsd.h"

#ifdef RULOS_ARM

#include "stm32f3xx_ll_bus.h"
#include "stm32f3xx_ll_spi.h"

// Called every 10ms.
static void ffs_update(void *data) {
  // This is a timer internal to the SD/FAT library; part of the API is that
  // we're required to do this every 10ms.
  if (ffs_10ms_timer) {
    ffs_10ms_timer--;
  }

  ffs_process();

  schedule_us(10000, ffs_update, NULL);
}

void ffs_init() {
  // Disable SPI1 so parameters can be changed
  LL_SPI_Disable(SPI1);

  // Enable GPIOB peripheral clock
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);

  // Configure SCK Pin
  LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_3, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetAFPin_0_7(GPIOB, LL_GPIO_PIN_3, LL_GPIO_AF_5);
  LL_GPIO_SetPinSpeed(GPIOB, LL_GPIO_PIN_3, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinPull(GPIOB, LL_GPIO_PIN_3, LL_GPIO_PULL_DOWN);

  // Configure MISO Pin
  LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_4, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetAFPin_0_7(GPIOB, LL_GPIO_PIN_4, LL_GPIO_AF_5);
  LL_GPIO_SetPinSpeed(GPIOB, LL_GPIO_PIN_4, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinPull(GPIOB, LL_GPIO_PIN_4, LL_GPIO_PULL_DOWN);

  // Configure MOSI Pin
  LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_5, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetAFPin_0_7(GPIOB, LL_GPIO_PIN_5, LL_GPIO_AF_5);
  LL_GPIO_SetPinSpeed(GPIOB, LL_GPIO_PIN_5, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinPull(GPIOB, LL_GPIO_PIN_5, LL_GPIO_PULL_DOWN);

  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
  LL_SPI_SetBaudRatePrescaler(SPI1, LL_SPI_BAUDRATEPRESCALER_DIV256);
  LL_SPI_SetTransferDirection(SPI1, LL_SPI_FULL_DUPLEX);
  LL_SPI_SetClockPhase(SPI1, LL_SPI_PHASE_1EDGE);
  LL_SPI_SetClockPolarity(SPI1, LL_SPI_POLARITY_LOW);
  LL_SPI_SetTransferBitOrder(SPI1, LL_SPI_MSB_FIRST);
  LL_SPI_SetDataWidth(SPI1, LL_SPI_DATAWIDTH_8BIT);
  LL_SPI_SetNSSMode(SPI1, LL_SPI_NSS_SOFT);
  LL_SPI_SetRxFIFOThreshold(SPI1, LL_SPI_RX_FIFO_TH_QUARTER);
  LL_SPI_SetMode(SPI1, LL_SPI_MODE_MASTER);

  // Enable SPI1!
  LL_SPI_Enable(SPI1);

  // Set chip-enable pin as an output
  gpio_make_output(GPIO_B6);

  // Kick off the 10ms-updater
  schedule_us(1, ffs_update, NULL);
}

#endif
