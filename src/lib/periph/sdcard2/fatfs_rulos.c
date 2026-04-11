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

#include "periph/sdcard2/fatfs_rulos.h"

#include "core/dma.h"
#include "core/hardware.h"
#include "core/rulos.h"

static void sdcard_dma_init(void);

////////////////////////////////////////////////////////////////
////// Implementation of the SD module's expecting down-facing API.

void FATFS_DEBUG_SEND_USART(const char* msg) {
  //LOG("SD card: %s", msg);
}

// 10ms-granularity timeout timer

// Absolute time of next timeout.
static Time timeout_time_us = 0;

void TM_DELAY_Init() {
}

void TM_DELAY_SetTime2(uint32_t timeout_ms) {
  timeout_time_us = clock_time_us() + (timeout_ms * 1000);
}

uint32_t TM_DELAY_Time2() {
  Time now = clock_time_us();
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

void TM_SPI_Init() {
  // Disable SD_SPI_PERIPH so parameters can be changed
  LL_SPI_Disable(SD_SPI_PERIPH);

  // Configure SCK Pin
  LL_GPIO_SetPinMode(SD_LL_SCK_PORT, SD_LL_SCK_PIN, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetAFPin_0_7(SD_LL_SCK_PORT, SD_LL_SCK_PIN, SD_LL_ALTFUNC);
  LL_GPIO_SetPinSpeed(SD_LL_SCK_PORT, SD_LL_SCK_PIN, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinPull(SD_LL_SCK_PORT, SD_LL_SCK_PIN, LL_GPIO_PULL_DOWN);

  // Configure MISO Pin
  LL_GPIO_SetPinMode(SD_LL_MISO_PORT, SD_LL_MISO_PIN, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetAFPin_0_7(SD_LL_MISO_PORT, SD_LL_MISO_PIN, SD_LL_ALTFUNC);
  LL_GPIO_SetPinSpeed(SD_LL_MISO_PORT, SD_LL_MISO_PIN, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinPull(SD_LL_MISO_PORT, SD_LL_MISO_PIN, LL_GPIO_PULL_DOWN);

  // Configure MOSI Pin
  LL_GPIO_SetPinMode(SD_LL_MOSI_PORT, SD_LL_MOSI_PIN, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetAFPin_0_7(SD_LL_MOSI_PORT, SD_LL_MOSI_PIN, SD_LL_ALTFUNC);
  LL_GPIO_SetPinSpeed(SD_LL_MOSI_PORT, SD_LL_MOSI_PIN, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinPull(SD_LL_MOSI_PORT, SD_LL_MOSI_PIN, LL_GPIO_PULL_DOWN);

  LL_APB2_GRP1_EnableClock(SD_LL_SPI_CLOCK);
  LL_SPI_SetBaudRatePrescaler(SD_SPI_PERIPH, LL_SPI_BAUDRATEPRESCALER_DIV256);
  LL_SPI_SetTransferDirection(SD_SPI_PERIPH, LL_SPI_FULL_DUPLEX);
  LL_SPI_SetClockPhase(SD_SPI_PERIPH, LL_SPI_PHASE_1EDGE);
  LL_SPI_SetClockPolarity(SD_SPI_PERIPH, LL_SPI_POLARITY_LOW);
  LL_SPI_SetTransferBitOrder(SD_SPI_PERIPH, LL_SPI_MSB_FIRST);
  LL_SPI_SetDataWidth(SD_SPI_PERIPH, LL_SPI_DATAWIDTH_8BIT);
  LL_SPI_SetNSSMode(SD_SPI_PERIPH, LL_SPI_NSS_SOFT);
  LL_SPI_SetRxFIFOThreshold(SD_SPI_PERIPH, LL_SPI_RX_FIFO_TH_QUARTER);
  LL_SPI_SetMode(SD_SPI_PERIPH, LL_SPI_MODE_MASTER);

  // Enable SD_SPI_PERIPH!
  LL_SPI_Enable(SD_SPI_PERIPH);

  // Set chip-enable pin as an output
  gpio_make_output(SD_PIN_CHIPENABLE);

  // Allocate DMA channels for SPI RX and TX. The actual mem_increment
  // setting flips between read and write operations, so the channels
  // are reconfigured via rulos_dma_reconfigure in each call to
  // TM_SPI_WriteMulti / TM_SPI_ReadMulti. The other config fields
  // (direction, width, priority, callbacks) stay constant.
#ifndef DO_NOT_USE_DMA
  sdcard_dma_init();
#endif
}

// void TM_SPI_SendMulti(SPI_TypeDef* SPIx, uint8_t* dataOut, uint8_t* dataIn,
// uint32_t count) {
//}

void TM_SPI_SetSlow() {
  LL_SPI_SetBaudRatePrescaler(SD_SPI_PERIPH, LL_SPI_BAUDRATEPRESCALER_DIV256);
}

void TM_SPI_SetFast() {
  LL_SPI_SetBaudRatePrescaler(SD_SPI_PERIPH, LL_SPI_BAUDRATEPRESCALER_DIV2);
}

//#define DO_NOT_USE_DMA

#ifdef DO_NOT_USE_DMA

// This is the old implementation of SPI writing, using a loop in code
// rather than DMA. Deprecated but still here in case we need it.
void TM_SPI_WriteMulti(SPI_TypeDef* SPIx, uint8_t* dataOut,
                              uint32_t count) {
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

// This is the old implementation of SPI reading, using a loop in code
// rather than DMA. Deprecated but still here in case we need it.
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

#else // DO_NOT_USE_DMA

static volatile bool transmissionComplete;
static rulos_dma_channel_t* rx_dma_ch;
static rulos_dma_channel_t* tx_dma_ch;

// Both channels' TC and error callbacks land here and just wake the
// busy-wait loop in dma_enable_and_wait. Runs in DMA ISR context.
// RX TC always fires last on a successful transfer (it has to wait
// for the last SPI clock cycle to shift in the MISO bit), so the
// normal path only needs RX. We wire TX too so that a TX DMA error
// doesn't deadlock the wait loop.
static void sdcard_dma_done_callback(void* user_data) {
  (void)user_data;
  transmissionComplete = true;
}

// Build a rulos_dma_config_t for either the RX or TX side of the SPI
// transaction. Only the varying fields are computed here; everything
// else is constant across reads and writes.
static rulos_dma_config_t make_rx_cfg(bool mem_increment) {
  return (rulos_dma_config_t){
      .request = RULOS_DMA_REQ_SPI1_RX,
      .direction = RULOS_DMA_DIR_PERIPH_TO_MEM,
      .mode = RULOS_DMA_MODE_NORMAL,
      .periph_width = RULOS_DMA_WIDTH_BYTE,
      .mem_width = RULOS_DMA_WIDTH_BYTE,
      .periph_increment = false,
      .mem_increment = mem_increment,
      .priority = RULOS_DMA_PRIORITY_HIGH,
      .tc_callback = sdcard_dma_done_callback,
      .error_callback = sdcard_dma_done_callback,
  };
}

static rulos_dma_config_t make_tx_cfg(bool mem_increment) {
  return (rulos_dma_config_t){
      .request = RULOS_DMA_REQ_SPI1_TX,
      .direction = RULOS_DMA_DIR_MEM_TO_PERIPH,
      .mode = RULOS_DMA_MODE_NORMAL,
      .periph_width = RULOS_DMA_WIDTH_BYTE,
      .mem_width = RULOS_DMA_WIDTH_BYTE,
      .periph_increment = false,
      .mem_increment = mem_increment,
      .priority = RULOS_DMA_PRIORITY_HIGH,
      .tc_callback = sdcard_dma_done_callback,
      .error_callback = sdcard_dma_done_callback,
  };
}

static void sdcard_dma_init(void) {
  // Default direction: write (RX discards to a dummy, TX increments
  // through source). The actual mem_increment flags are reconfigured
  // on every call to TM_SPI_WriteMulti / TM_SPI_ReadMulti.
  const rulos_dma_config_t rx_cfg = make_rx_cfg(false);
  rx_dma_ch = rulos_dma_alloc(&rx_cfg);
  if (rx_dma_ch == NULL) {
    __builtin_trap();
  }

  const rulos_dma_config_t tx_cfg = make_tx_cfg(true);
  tx_dma_ch = rulos_dma_alloc(&tx_cfg);
  if (tx_dma_ch == NULL) {
    __builtin_trap();
  }
}

static void dma_enable_and_wait(void* rx_mem, void* tx_mem, uint32_t count) {
  volatile void* const spi_dr =
      (volatile void*)LL_SPI_DMA_GetRegAddr(SD_SPI_PERIPH);

  transmissionComplete = false;

  rulos_dma_start(rx_dma_ch, spi_dr, rx_mem, count);
  rulos_dma_start(tx_dma_ch, spi_dr, tx_mem, count);

  LL_SPI_EnableDMAReq_RX(SD_SPI_PERIPH);
  LL_SPI_EnableDMAReq_TX(SD_SPI_PERIPH);

  // Block until a DMA callback fires (TC on successful completion,
  // or error_callback on DMA error on either channel).
  while (!transmissionComplete) {
    __WFI();
  }

  LL_SPI_DisableDMAReq_RX(SD_SPI_PERIPH);
  LL_SPI_DisableDMAReq_TX(SD_SPI_PERIPH);
  rulos_dma_stop(rx_dma_ch);
  rulos_dma_stop(tx_dma_ch);
}

void TM_SPI_WriteMulti(SPI_TypeDef* SPIx, uint8_t* dataOut, uint32_t count) {
  uint8_t rx_sink;

  // RX side discards incoming bytes into a single dummy location
  // (mem_increment=false). TX side walks through dataOut
  // (mem_increment=true).
  const rulos_dma_config_t rx_cfg = make_rx_cfg(false);
  rulos_dma_reconfigure(rx_dma_ch, &rx_cfg);
  const rulos_dma_config_t tx_cfg = make_tx_cfg(true);
  rulos_dma_reconfigure(tx_dma_ch, &tx_cfg);

  dma_enable_and_wait(&rx_sink, dataOut, count);
}

void TM_SPI_ReadMulti(SPI_TypeDef* SPIx, uint8_t* dataIn, uint8_t dummy,
                      uint32_t count) {
  // RX side walks through dataIn (mem_increment=true). TX side reads
  // the same dummy byte over and over (mem_increment=false) to clock
  // the SPI bus.
  const rulos_dma_config_t rx_cfg = make_rx_cfg(true);
  rulos_dma_reconfigure(rx_dma_ch, &rx_cfg);
  const rulos_dma_config_t tx_cfg = make_tx_cfg(false);
  rulos_dma_reconfigure(tx_dma_ch, &tx_cfg);

  dma_enable_and_wait(dataIn, &dummy, count);
}

#endif

///////////////////////////

// Implementation of FATFS's down-facing API for RULOS, hardwired to the SD
// card. Note the original SD card library.

DSTATUS disk_initialize(BYTE pdrv) {
  return TM_FATFS_SD_disk_initialize();
}

DSTATUS disk_status(BYTE pdrv) {
  return TM_FATFS_SD_disk_status();
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
  return TM_FATFS_SD_disk_read(buff, sector, count);
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
  return TM_FATFS_SD_disk_write(buff, sector, count);
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
  return TM_FATFS_SD_disk_ioctl(cmd, buff);
}
