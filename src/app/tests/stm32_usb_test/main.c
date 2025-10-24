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
#include "periph/uart/uart.h"
#include "usbd_cdc.h"
#include "usbd_conf.h"
#include <stdio.h>

USBD_HandleTypeDef hUsbDeviceFS;
extern USBD_DescriptorsTypeDef CDC_Desc;

char UserTxBuffer[2048];
uint8_t UserRxBuffer[CDC_DATA_FS_MAX_PACKET_SIZE];

// Track whether we're ready to transmit
static bool usb_ready = false;
static bool usb_tx_busy = false;

// UART for logging
UartState_t uart;

// Buffer for received data (copy from ISR to main loop)
static uint8_t rx_log_buffer[CDC_DATA_FS_MAX_PACKET_SIZE];
static volatile uint32_t rx_log_len = 0;

// Static buffer for USB CDC handle allocation
// The USB library allocates USBD_CDC_HandleTypeDef during initialization
static uint8_t malloc_mem[sizeof(USBD_CDC_HandleTypeDef)] __attribute__((aligned(4)));
static bool malloc_allocated = false;

void *USBD_malloc(uint32_t size) {
  if (malloc_allocated) {
    __builtin_trap();
  }
  if (size > sizeof(malloc_mem)) {
    __builtin_trap();
  }
  malloc_allocated = true;
  return malloc_mem;
}

void USBD_free(void *ptr) {
  if (!malloc_allocated) {
    __builtin_trap();
  }
  malloc_allocated = false;
}

static int8_t CDC_Init_FS(void)
{
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBuffer);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);

  return (USBD_OK);
}

static int8_t CDC_DeInit_FS(void)
{
  return USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  // When host opens the port, mark USB as ready
  if (cmd == CDC_SET_CONTROL_LINE_STATE) {
    usb_ready = true;
  }

  return USBD_OK;
}

static void log_rx_task(void *data);

static int8_t CDC_Receive_FS(uint8_t* buf, uint32_t *len)
{
  // Copy data for logging in main loop (can't LOG from ISR)
  // Drop packets if previous one hasn't been processed yet
  if (*len > 0 && rx_log_len == 0) {
    // Must copy the data before preparing the next receive!
    // The USB library will reuse the buffer immediately.
    uint32_t copy_len = (*len < sizeof(rx_log_buffer)) ? *len : sizeof(rx_log_buffer);
    memcpy(rx_log_buffer, buf, copy_len);

    // Set length last - this signals to main loop that data is ready
    __DMB();  // Data memory barrier to ensure copy completes
    rx_log_len = copy_len;

    // Schedule the print task to run
    schedule_now(log_rx_task, NULL);
  }

  // Prepare to receive more data
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return USBD_OK;
}

static int8_t CDC_TransmitComplete(uint8_t* buf, uint32_t *len, uint8_t epnum) {
  usb_tx_busy = false;
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



void init_usb() {
   RCC_CRSInitTypeDef RCC_CRSInitStruct= {0};
   RCC_OscInitTypeDef RCC_OscInitStruct = {0};

   LOG("init_usb: Starting USB initialization");

   /* Enable HSI48 oscillator */
   RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
   RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
   RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
   if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
     LOG("init_usb: Failed to enable HSI48");
     __builtin_trap();
   }
   LOG("init_usb: HSI48 enabled");

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

  LOG("init_usb: CRS configured");

  /* Init Device Library, add supported class and start the library. */
  LOG("init_usb: Calling USBD_Init");
  if (USBD_Init(&hUsbDeviceFS, &CDC_Desc, 0) != USBD_OK) {
    LOG("init_usb: USBD_Init FAILED");
    __builtin_trap();
  }
  LOG("init_usb: Calling USBD_RegisterClass");
  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK) {
    LOG("init_usb: USBD_RegisterClass FAILED");
    __builtin_trap();
  }
  LOG("init_usb: Calling USBD_CDC_RegisterInterface");
  if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) != USBD_OK) {
    LOG("init_usb: USBD_CDC_RegisterInterface FAILED");
    __builtin_trap();
  }
  LOG("init_usb: Calling USBD_Start");
  if (USBD_Start(&hUsbDeviceFS) != USBD_OK) {
    LOG("init_usb: USBD_Start FAILED");
    __builtin_trap();
  }
  LOG("init_usb: USB initialization complete");

  // Flush UART before USB interrupts start firing
  rulos_uart_flush(&uart);
  /* USER CODE BEGIN USB_Device_Init_PostTreatment */

  /* USER CODE END USB_Device_Init_PostTreatment */

}

//////////////////////////////////////////////////////////////////////////////
// Periodic task to send hello messages
//////////////////////////////////////////////////////////////////////////////

static void log_rx_task(void *data) {
  // Check if we have received data to log
  uint8_t local_buf[CDC_DATA_FS_MAX_PACKET_SIZE];
  uint32_t len;

  // Critical section: read length and copy buffer atomically
  rulos_irq_state_t old_irq = hal_start_atomic();
  len = rx_log_len;
  if (len > 0) {
    memcpy(local_buf, rx_log_buffer, len);
    rx_log_len = 0;  // Signal ISR that buffer is available
  }
  hal_end_atomic(old_irq);

  // Log received data
  if (len > 0) {
    LOG("USB RX: %lu bytes: '%.*s'", len, (int)len, local_buf);
  }
}

static void hello_task(void *data) {
  static uint32_t message_counter = 0;

  // Schedule next message in 1 second
  schedule_us(1000000, hello_task, NULL);

  // Don't send if not ready or still transmitting
  if (!usb_ready || usb_tx_busy) {
    return;
  }

  // Format message directly into TX buffer
  int len = snprintf((char *)UserTxBuffer, sizeof(UserTxBuffer),
                     "Hello from STM32G4! Message #%lu uptime=%lu ms\r\n",
                     message_counter++,
                     clock_time_us() / 1000);

  if (len > 0 && len < sizeof(UserTxBuffer)) {
    usb_tx_busy = true;
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, (uint8_t *)UserTxBuffer, len);
    USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  }
}

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER1);

  uart_init(&uart, /* uart_id= */ 0, 1000000);
  log_bind_uart(&uart);

  LOG("STM32G4 USB CDC test starting up!");

  init_usb();

  // Send initial greeting after 2 seconds (give USB time to enumerate)
  schedule_us(2000000, hello_task, NULL);

  scheduler_run();
}
