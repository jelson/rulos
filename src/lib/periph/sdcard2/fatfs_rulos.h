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

// RULOS NOTE:
// This is 3 layers of software I (jelson@) have imported from 3rd parties--
//
// 1) FatFS, http://elm-chan.org/fsw/ff/00index_e.html, which
// implements FAT only (ff.h, ff.c, ffconf.h, ffsystem.c,
// ffunicode.c), and an interface for disk operations (diskio.h).
//
// 2) The SD card implementation of the FatFS disk io interface for
// STM32 (fatfs_sd.c), from
// https://github.com/MaJerle/stm32f429/tree/master/00-STM32F429_LIBRARIES/fatfs/drivers
// with some local modifications by jelson@.
//
// 3) The RULOS-specific support functions for talking to timers and
// the SPI hardware expected by fatfs_sd.c. The SD card library we're
// using is part of a large framework, sort of an operating system of
// its own, that defines many services -- including countdown
// timers. fatfs_rulos.h and fatfs_rulos.c implement the expected APIs
// from within the RULOS framework.

#pragma once

#include <stdint.h>

#include "core/hardware.h"
#include "stm32f3xx_ll_spi.h"

#include "periph/fatfs/diskio.h"
#include "periph/fatfs/ff.h"
#include "periph/sdcard2/fatfs_rulos.h"

#define _USE_WRITE 1 /* 1: Enable disk_write function */
#define _USE_IOCTL 1 /* 1: Enable disk_ioctl fucntion */

#define FATFS_SPI SPI1

#ifndef FATFS_USE_DETECT_PIN
#define FATFS_USE_DETECT_PIN 0
#endif

#ifndef FATFS_USE_WRITEPROTECT_PIN
#define FATFS_USE_WRITEPROTECT_PIN 0
#endif

#if FATFS_USE_DETECT_PIN > 0
#ifndef FATFS_USE_DETECT_PIN_PIN
#define FATFS_USE_DETECT_PIN_PORT GPIOB
#define FATFS_USE_DETECT_PIN_PIN GPIO_PIN_6
#endif
#endif

#if FATFS_USE_WRITEPROTECT_PIN > 0
#ifndef FATFS_USE_WRITEPROTECT_PIN_PIN
#define FATFS_USE_WRITEPROTECT_PIN_RCC RCC_AHB1Periph_GPIOB
#define FATFS_USE_WRITEPROTECT_PIN_PORT GPIOB
#define FATFS_USE_WRITEPROTECT_PIN_PIN GPIO_Pin_7
#endif
#endif

/////////////////////////////////////////////////////////////////////////////

#define SD_PIN_CHIPENABLE GPIO_B6

#define FATFS_CS_LOW gpio_clr(SD_PIN_CHIPENABLE)
#define FATFS_CS_HIGH gpio_set(SD_PIN_CHIPENABLE)

void TM_DELAY_Init();
void TM_DELAY_SetTime2(uint32_t timeout_time_ms);
uint32_t TM_DELAY_Time2();
void TM_SPI_Init();
void FATFS_DEBUG_SEND_USART(const char* msg);
void TM_SPI_SetSlow();
void TM_SPI_SetFast();

#define SPI_WAIT(SPIx) \
  do {                 \
  } while (SPIx->SR & SPI_SR_BSY)

static __INLINE uint8_t TM_SPI_Send(SPI_TypeDef* SPIx, uint8_t data) {
  /* Wait for previous transmissions to complete if DMA TX enabled for SPI */
  SPI_WAIT(SPIx);

  LL_SPI_TransmitData8(SPIx, data);

  /* Wait for transmission to complete */
  SPI_WAIT(SPIx);

  /* Return data from buffer */
  return SPIx->DR;
}

/**
 * @brief  Sends and receives multiple bytes over SPIx
 * @param  *SPIx: Pointer to SPIx peripheral you will use, where x is between 1
 * to 6
 * @param  *dataOut: Pointer to array with data to send over SPI
 * @param  *dataIn: Pointer to array to to save incoming data
 * @param  count: Number of bytes to send/receive over SPI
 * @retval None
 */
void TM_SPI_SendMulti(SPI_TypeDef* SPIx, uint8_t* dataOut, uint8_t* dataIn,
                      uint32_t count);

/**
 * @brief  Writes multiple bytes over SPI
 * @param  *SPIx: Pointer to SPIx peripheral you will use, where x is between 1
 * to 6
 * @param  *dataOut: Pointer to array with data to send over SPI
 * @param  count: Number of elements to send over SPI
 * @retval None
 */
void TM_SPI_WriteMulti(SPI_TypeDef* SPIx, uint8_t* dataOut, uint32_t count);

/**
 * @brief  Receives multiple data bytes over SPI
 * @note   Selected SPI must be set in 16-bit mode
 * @param  *SPIx: Pointer to SPIx peripheral you will use, where x is between 1
 * to 6
 * @param  *dataIn: Pointer to 8-bit array to save data into
 * @param  dummy: Dummy byte  to be sent over SPI, to receive data back. In most
 * cases 0x00 or 0xFF
 * @param  count: Number of bytes you want read from device
 * @retval None
 */
void TM_SPI_ReadMulti(SPI_TypeDef* SPIx, uint8_t* dataIn, uint8_t dummy,
                      uint32_t count);

DSTATUS TM_FATFS_SD_disk_initialize(void);

DSTATUS TM_FATFS_SD_disk_status(void);

DRESULT TM_FATFS_SD_disk_read(
    BYTE* buff,   /* Data buffer to store read data */
    DWORD sector, /* Sector address (LBA) */
    UINT count    /* Number of sectors to read (1..128) */
);

#if _USE_WRITE
DRESULT TM_FATFS_SD_disk_write(
    const BYTE* buff, /* Data to be written */
    DWORD sector,     /* Sector address (LBA) */
    UINT count        /* Number of sectors to write (1..128) */
);
#endif

#if _USE_IOCTL
DRESULT TM_FATFS_SD_disk_ioctl(
    BYTE cmd,  /* Control code */
    void* buff /* Buffer to send/receive control data */
);
#endif
