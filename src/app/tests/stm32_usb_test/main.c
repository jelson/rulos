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

#include "core/hardware.h"
#include "core/rulos.h"
#include "usbd_cdc.h"
#include "usbd_conf.h"
#include <stdio.h>
#include <stdarg.h>

USBD_HandleTypeDef hUsbDeviceFS;
extern USBD_DescriptorsTypeDef CDC_Desc;

uint8_t UserTxBuffer[2048];
uint8_t UserRxBuffer[CDC_DATA_FS_MAX_PACKET_SIZE];

// Track whether we're ready to transmit
static bool usb_ready = false;

static USBD_HandleTypeDef malloc_mem;
static bool malloc_allocated = false;

void *USBD_malloc(uint32_t size) {
  if (malloc_allocated) {
    __builtin_trap();
  }
  if (size != sizeof(malloc_mem)) {
    __builtin_trap();
  }
  malloc_allocated = true;
  return &malloc_mem;
}

void USBD_free(void *ptr) {
  if (!malloc_allocated) {
    __builtin_trap();
  }
  malloc_allocated = false;
}

static int8_t CDC_Init_FS(void)
{
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBuffer, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBuffer);

  return (USBD_OK);
}

static int8_t CDC_DeInit_FS(void)
{
  return USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  LOG("got usb control command %d", cmd);

  // When host opens the port, mark USB as ready
  if (cmd == CDC_SET_CONTROL_LINE_STATE) {
    usb_ready = true;
    LOG("USB CDC ready!");
  }

  return USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t* buf, uint32_t *len)
{
  LOG("got %d bytes:", *len);
  log_write(buf, *len);

  // Echo the received data back
  if (usb_ready && *len > 0) {
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, buf, *len);
    USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  }

  // Prepare to receive more data
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return USBD_OK;
}

static int8_t CDC_TransmitComplete(uint8_t* buf, uint32_t *len, uint8_t epnum) {
  return USBD_OK;
}


USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS,
  CDC_TransmitComplete,
};

//////////////////////////////////////////////////////////////////////////////
// Helper API for easy USB CDC transmission
//////////////////////////////////////////////////////////////////////////////

// Send a string over USB CDC
int usb_cdc_puts(const char *str) {
  if (!usb_ready) {
    return -1;  // Not ready
  }

  uint16_t len = 0;
  while (str[len] != '\0' && len < sizeof(UserTxBuffer)) {
    len++;
  }

  if (len == 0) {
    return 0;
  }

  // Copy to tx buffer
  for (uint16_t i = 0; i < len; i++) {
    UserTxBuffer[i] = str[i];
  }

  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBuffer, len);
  uint8_t result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);

  return (result == USBD_OK) ? len : -1;
}

// Printf-style function for USB CDC
int usb_cdc_printf(const char *format, ...) {
  if (!usb_ready) {
    return -1;
  }

  va_list args;
  va_start(args, format);
  int len = vsnprintf((char *)UserTxBuffer, sizeof(UserTxBuffer), format, args);
  va_end(args);

  if (len <= 0 || len >= sizeof(UserTxBuffer)) {
    return -1;
  }

  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBuffer, len);
  uint8_t result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);

  return (result == USBD_OK) ? len : -1;
}


void init_usb() {
   RCC_CRSInitTypeDef RCC_CRSInitStruct= {0};

   /*Enable CRS Clock*/
  __HAL_RCC_CRS_CLK_ENABLE();

  /* Default Synchro Signal division factor (not divided) */
  RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;

  /* Set the SYNCSRC[1:0] bits according to CRS_Source value */
  RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB;

  /* HSI48 is synchronized with USB SOF at 1KHz rate */
  RCC_CRSInitStruct.ReloadValue =  __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
  RCC_CRSInitStruct.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;

  /* Set the TRIM[5:0] to the default value */
  RCC_CRSInitStruct.HSI48CalibrationValue = RCC_CRS_HSI48CALIBRATION_DEFAULT;

  /* Start automatic synchronization */
  HAL_RCCEx_CRSConfig (&RCC_CRSInitStruct);

  

  /* Init Device Library, add supported class and start the library. */
  if (USBD_Init(&hUsbDeviceFS, &CDC_Desc, 0) != USBD_OK) {
    __builtin_trap();
  }
  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK) {
    __builtin_trap();
  }
  if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) != USBD_OK) {
    __builtin_trap();
  }
  if (USBD_Start(&hUsbDeviceFS) != USBD_OK) {
    __builtin_trap();
  }
  /* USER CODE BEGIN USB_Device_Init_PostTreatment */

  /* USER CODE END USB_Device_Init_PostTreatment */

}

//////////////////////////////////////////////////////////////////////////////
// Periodic task to send hello messages
//////////////////////////////////////////////////////////////////////////////

static uint32_t message_counter = 0;

static void hello_task(void *data) {
  // Send a periodic hello message
  usb_cdc_printf("Hello from STM32G4! Message #%lu uptime=%lu ms\r\n",
                 message_counter++,
                 clock_time_us() / 1000);

  // Schedule next message in 1 second
  schedule_us(1000000, hello_task, NULL);
}

int main() {
  rulos_hal_init();
  init_usb();

  // Send initial greeting after 2 seconds (give USB time to enumerate)
  schedule_us(2000000, hello_task, NULL);

  scheduler_run();
}
