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

#include "periph/fatfs/fatfs_rulos.h"

#include "core/hardware.h"
#include "core/rulos.h"

////////////////////////////////////////////////////////////////
////// Implementation of the SD module's expecting down-facing API.

void FATFS_DEBUG_SEND_USART(const char* msg) {
  // LOG("SD card: %s", msg);
}

// 10ms-granularity timeout timer

// Absolute time of next timeout.
static uint32_t timeout_time_us = 0;

void TM_DELAY_Init() {}

void TM_DELAY_SetTime2(uint32_t timeout_ms) {
  timeout_time_us = get_interrupt_driven_jiffy_clock() + (timeout_ms * 1000);
}

uint32_t TM_DELAY_Time2() {
  Time now = get_interrupt_driven_jiffy_clock();
  Time remaining = (timeout_time_us - now) / 1000;

  if (remaining > 0) {
    return remaining;
  } else {
    // If the timeout has already expired, move the timeout time
    // forward to avoid rollover problems
    timeout_time_us = now;
    return 0;
  }
}

#ifdef RULOS_ARM

#include "stm32f3xx_ll_bus.h"
#include "stm32f3xx_ll_spi.h"

void TM_SPI_Init() {
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
  gpio_make_output(SD_PIN_CHIPENABLE);
}

// void TM_SPI_SendMulti(SPI_TypeDef* SPIx, uint8_t* dataOut, uint8_t* dataIn,
// uint32_t count) {
//}

void TM_SPI_SetSlow() {
  LL_SPI_SetBaudRatePrescaler(SPI1, LL_SPI_BAUDRATEPRESCALER_DIV256);
}

void TM_SPI_SetFast() {
  LL_SPI_SetBaudRatePrescaler(SPI1, LL_SPI_BAUDRATEPRESCALER_DIV2);
}

void TM_SPI_WriteMulti(SPI_TypeDef* SPIx, uint8_t* dataOut, uint32_t count) {
  uint32_t i;

  /* Wait for previous transmissions to complete if DMA TX enabled for SPI */
  SPI_WAIT(SPIx);

  for (i = 0; i < count; i++) {
    /* Fill output buffer with data */
    LL_SPI_TransmitData8(SPIx, dataOut[i]);

    /* Wait for SPI to end everything */
    SPI_WAIT(SPIx);

    /* Read data register */
    (void)SPIx->DR;
  }
}

void TM_SPI_ReadMulti(SPI_TypeDef* SPIx, uint8_t* dataIn, uint8_t dummy,
                      uint32_t count) {
  uint32_t i;

  /* Wait for previous transmissions to complete if DMA TX enabled for SPI */
  SPI_WAIT(SPIx);

  for (i = 0; i < count; i++) {
    /* Fill output buffer with data */
    LL_SPI_TransmitData8(SPIx, dummy);

    /* Wait for SPI to end everything */
    SPI_WAIT(SPIx);

    /* Save data to buffer */
    *dataIn++ = LL_SPI_ReceiveData8(SPIx);
  }
}

#endif

///////////////////////////

// Implementation of FATFS's down-facing API for RULOS, hardwired to the SD
// card. Note the original SD card library.

DSTATUS disk_initialize(BYTE pdrv) { return TM_FATFS_SD_disk_initialize(); }

DSTATUS disk_status(BYTE pdrv) { return TM_FATFS_SD_disk_status(); }

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
  return TM_FATFS_SD_disk_read(buff, sector, count);
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
  return TM_FATFS_SD_disk_write(buff, sector, count);
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
  return TM_FATFS_SD_disk_ioctl(cmd, buff);
}
