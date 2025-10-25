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

#include "usb_cdc.h"
#include "core/hal.h"
#include "core/rulos.h"
#include "stm32.h"
#include "usbd_cdc.h"
#include "usbd_conf.h"
#include "usbd_def.h"
#include <string.h>

// External descriptor defined in usb_desc.c
extern USBD_DescriptorsTypeDef CDC_Desc;

// Pointer to the single CDC device state -- currently a singleton
static usbd_cdc_state_t *cdc_device = NULL;

// USBD_malloc/free stubs required by USB middleware. We expect only one malloc:
// for the CDC handle, which we embed in our usbd_cdc_state_t structure.
void *USBD_malloc(uint32_t size) {
  if (cdc_device == NULL) {
    __builtin_trap();  // No device initialized
  }
  if (size != sizeof(cdc_device->cdc_handle)) {
    __builtin_trap();  // Requested size unexpected
  }

  return &cdc_device->cdc_handle;
}

void USBD_free(void *ptr) {
  // NULL is valid to free (no-op)
  if (ptr == NULL) {
    return;
  }

  // Verify it's pointing to our embedded handle
  if (cdc_device == NULL || ptr != &cdc_device->cdc_handle) {
    __builtin_trap();  // Invalid pointer
  }

  // Nothing to do since it's statically allocated
}


// Task to deliver RX data to application callback (can't call from ISR)
static void rx_delivery_task(void *data) {
  usbd_cdc_state_t *cdc = (usbd_cdc_state_t *)data;

  // Deliver to application callback
  // Buffer is valid until next USBD_CDC_ReceivePacket() call
  if (cdc->rx_pending_len > 0 && cdc->rx_cb) {
    cdc->rx_cb(cdc, cdc->user_data, cdc->rx_buf, cdc->rx_pending_len);
    cdc->rx_pending_len = 0;
  }

  // Prepare to receive more data
  USBD_CDC_ReceivePacket(&cdc->usbd_handle);
}

//////////////////////////////////////////////////////////////////////////////
// CDC Interface callbacks (called by USB middleware)
//////////////////////////////////////////////////////////////////////////////

static int8_t CDC_Init_FS(void) {
  // Called when USB CDC interface is initialized
  if (cdc_device) {
    USBD_CDC_SetRxBuffer(&cdc_device->usbd_handle, cdc_device->rx_buf);
    USBD_CDC_ReceivePacket(&cdc_device->usbd_handle);
  }
  return USBD_OK;
}

static int8_t CDC_DeInit_FS(void) {
  return USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length) {
  if (cdc_device == NULL) {
    LOG("CDC_Control_FS: no device");
    return USBD_FAIL;
  }
  // When host opens the port, mark USB as ready
  if (cmd == CDC_SET_CONTROL_LINE_STATE) {
    cdc_device->usb_ready = true;
  }
  return USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t *buf, uint32_t *len) {
  if (cdc_device == NULL) {
    LOG("CDC_Receive_FS: no device");
    return USBD_FAIL;
  }

  // Verify buffer address matches (sanity check)
  if (cdc_device->rx_buf != buf) {
    LOG("CDC_Receive_FS: buffer mismatch");
    return USBD_FAIL;
  }

  // If data received, deliver to application
  if (*len > 0) {
    // buf already points to cdc_device->rx_buf (set in CDC_Init_FS)
    cdc_device->rx_pending_len = *len;

    // Deliver to application (runs synchronously)
    schedule_now(rx_delivery_task, cdc_device);
  }

  return USBD_OK;
}

static int8_t CDC_TransmitComplete(uint8_t *buf, uint32_t *len, uint8_t epnum) {
  if (cdc_device == NULL) {
    LOG("CDC_TransmitComplete: no device");
    return USBD_FAIL;
  }

  // Verify buffer address matches (sanity check)
  if (cdc_device->tx_buf_in_flight != buf) {
    LOG("CDC_TransmitComplete: buffer mismatch");
    return USBD_FAIL;
  }

  cdc_device->tx_busy = false;

  // Notify application
  if (cdc_device->tx_complete_cb) {
    cdc_device->tx_complete_cb(cdc_device, cdc_device->user_data);
  }

  return USBD_OK;
}

// CDC Interface function pointers
USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS,
  CDC_TransmitComplete,
};

//////////////////////////////////////////////////////////////////////////////
// Clock configuration for USB (STM32G4 specific - uses HSI48)
//////////////////////////////////////////////////////////////////////////////

static void init_usb_clock(void) {
#if defined(RULOS_ARM_stm32g4) || defined(RULOS_ARM_stm32g0)
  RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};

  // Enable HSI48 oscillator (48 MHz for USB)
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    __builtin_trap();
  }

  // Enable CRS (Clock Recovery System) for HSI48
  __HAL_RCC_CRS_CLK_ENABLE();

  RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
  RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB;
  RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
  RCC_CRSInitStruct.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;
  RCC_CRSInitStruct.HSI48CalibrationValue = RCC_CRS_HSI48CALIBRATION_DEFAULT;

  HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);
#else
  // TODO: Add clock configuration for other STM32 families
  #error "USB clock configuration not implemented for this STM32 family"
#endif
}

//////////////////////////////////////////////////////////////////////////////
// Public API
//////////////////////////////////////////////////////////////////////////////

void usbd_cdc_init(usbd_cdc_state_t *cdc) {
  // Only one CDC device supported - check if already initialized
  if (cdc_device != NULL) {
    __builtin_trap();  // Already have a CDC device initialized
  }

  // Initialize state
  cdc->initted = false;
  cdc->usb_ready = false;
  cdc->tx_busy = false;
  cdc->rx_pending_len = 0;
  cdc->tx_buf_in_flight = NULL;

  // Set global device pointer BEFORE any USB calls
  // This ensures callbacks can find the device even during USB init
  cdc_device = cdc;

  // Configure USB clock
  init_usb_clock();

  // Initialize USB device library using embedded handle
  if (USBD_Init(&cdc->usbd_handle, &CDC_Desc, 0) != USBD_OK) {
    __builtin_trap();
  }

  if (USBD_RegisterClass(&cdc->usbd_handle, &USBD_CDC) != USBD_OK) {
    __builtin_trap();
  }

  if (USBD_CDC_RegisterInterface(&cdc->usbd_handle, &USBD_Interface_fops_FS) != USBD_OK) {
    __builtin_trap();
  }

  if (USBD_Start(&cdc->usbd_handle) != USBD_OK) {
    __builtin_trap();
  }

  cdc->initted = true;
}

bool usbd_cdc_tx_ready(usbd_cdc_state_t *cdc) {
  return cdc->initted && cdc->usb_ready && !cdc->tx_busy;
}

int usbd_cdc_write(usbd_cdc_state_t *cdc, const void *buf, uint32_t len) {
  if (!usbd_cdc_tx_ready(cdc)) {
    LOG("usbd_cdc_write: trying to write when not ready (initted=%d usb_ready=%d tx_busy=%d)",
        cdc->initted, cdc->usb_ready, cdc->tx_busy);
    return -1;
  }

  cdc->tx_busy = true;
  cdc->tx_buf_in_flight = buf;

  USBD_CDC_SetTxBuffer(&cdc->usbd_handle, (uint8_t *)buf, len);
  USBD_CDC_TransmitPacket(&cdc->usbd_handle);

  return 0;
}
