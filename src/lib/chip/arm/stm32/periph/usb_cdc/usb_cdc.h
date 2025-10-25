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
 * STM32 USB CDC (Virtual COM Port) Peripheral
 *
 * Only single CDC device supported (not composite device with multiple CDC ports).
 *
 * USB descriptor strings and configuration can be customized at compile time
 * by adding defines to your SConstruct extra_cflags:
 *
 * Example:
 *   extra_cflags = [
 *     '-DUSBD_VID=0x1234',
 *     '-DUSBD_PID=0x5678',
 *     '-DUSBD_MANUFACTURER_STRING=\\"My Company\\"',
 *     '-DUSBD_PRODUCT_STRING=\\"My Device\\"',
 *     '-DUSBD_SERIAL_STRING=\\"SN123456\\"',
 *   ]
 *
 * Defaults:
 *   VID: 0x0424 (Microchip Technology, Inc.)
 *   PID: 0x274e (Gets Linux DISABLE_ECHO quirk - see link below)
 *   Manufacturer: "Lectrobox"
 *   Product: "STM32 Virtual ComPort"
 *   Serial: "00000000001A"
 *
 * Default VID/PID chosen to avoid garbled output on Linux (DISABLE_ECHO quirk).
 * See: https://michael.stapelberg.ch/posts/2021-04-27-linux-usb-virtual-serial-cdc-acm/
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "usbd_def.h"
#include "usbd_cdc.h"

// Forward declarations
struct usbd_cdc_state_s;
typedef struct usbd_cdc_state_s usbd_cdc_state_t;

// Callbacks
typedef void (*usbd_cdc_rx_cb)(usbd_cdc_state_t *cdc, void *user_data,
                               const uint8_t *data, uint32_t len);
typedef void (*usbd_cdc_tx_complete_cb)(usbd_cdc_state_t *cdc, void *user_data);

// State structure - caller allocates this
struct usbd_cdc_state_s {
  // Public: caller must set these before init
  usbd_cdc_rx_cb rx_cb;
  usbd_cdc_tx_complete_cb tx_complete_cb;
  void *user_data;

  // Private: library manages these - do not access directly

  bool initted;
  bool usb_ready;
  bool tx_busy;

  // RX buffer - USB Full Speed max packet size (64 bytes)
  uint8_t rx_buf[64];
  uint32_t rx_pending_len;

  // pointer to pending TX
  const void *tx_buf_in_flight;

  // STM32 HAL handles
  USBD_HandleTypeDef usbd_handle;       // USB device handle
  USBD_CDC_HandleTypeDef cdc_handle;    // CDC class handle
};

// Initialize USB CDC device (configures clocks, USB peripheral, descriptors)
void usbd_cdc_init(usbd_cdc_state_t *cdc);

// Transmit data from caller's buffer.
// Buffer must remain valid until tx_complete callback is invoked
// Returns 0 on success, -1 if previous transmission still in progress
int usbd_cdc_write(usbd_cdc_state_t *cdc, const void *buf, uint32_t len);

// Check if ready to transmit (true if USB enumerated and not busy)
bool usbd_cdc_tx_ready(usbd_cdc_state_t *cdc);
