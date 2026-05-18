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
#include "core/dfu.h"
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

static void disconnect_task(void *data);

static int8_t CDC_DeInit_FS(void) {
  if (cdc_device) {
    bool was_ready = cdc_device->usb_ready;
    cdc_device->usb_ready = false;
    // If USB drops mid-transfer, TransmitComplete will never fire.
    // Reset tx state so the device can TX again after reconnect.
    cdc_device->tx_busy = false;
    cdc_device->tx_buf_in_flight = NULL;
    if (was_ready) {
      schedule_now(disconnect_task, cdc_device);
    }
  }
  return USBD_OK;
}

// The RULOS DFU runtime interface is interface #2 in the CDC config
// descriptor (see ext/.../Class/CDC/Src/usbd_cdc.c). A host running
// `dfu-util -e` sends the DFU_DETACH class request (bRequest 0, zero
// length) to it; we reboot into the ROM bootloader (core/dfu.c).
#define DFU_RUNTIME_IFACE 2U
#define DFU_DETACH_REQUEST 0x00U

// Deferred so the DFU_DETACH control transfer's status stage can ACK
// before the device drops off the bus -- otherwise dfu-util reports a
// failed detach even though the reboot is happening.
static void dfu_detach_task(void *data) {
  rulos_dfu_enter_bootloader();  // sets magic + NVIC_SystemReset(); no return
}

// Host open/close (DTR edge) delivered from task context, not the USB
// callback context, like rx/tx.
static void connect_task(void *data) {
  usbd_cdc_state_t *cdc = (usbd_cdc_state_t *)data;
  if (cdc->connect_cb) {
    cdc->connect_cb(cdc, cdc->user_data);
  }
}
static void disconnect_task(void *data) {
  usbd_cdc_state_t *cdc = (usbd_cdc_state_t *)data;
  if (cdc->disconnect_cb) {
    cdc->disconnect_cb(cdc, cdc->user_data);
  }
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length) {
  if (cdc_device == NULL) {
    LOG("CDC_Control_FS: no device");
    return USBD_FAIL;
  }
  // Zero-length class requests arrive with pbuf = the raw setup packet.
  // DFU_DETACH targets the DFU interface; the wIndex guard keeps it
  // from colliding with the CDC class requests (0x20..0x22).
  if (cmd == DFU_DETACH_REQUEST) {
    USBD_SetupReqTypedef *req = (USBD_SetupReqTypedef *)pbuf;
    if ((req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS &&
        req->wIndex == DFU_RUNTIME_IFACE) {
      schedule_us(100000, dfu_detach_task, NULL);
      return USBD_OK;
    }
  }
  // Track host port open/close via DTR bit in SET_CONTROL_LINE_STATE.
  // When wLength==0 the middleware passes the raw setup packet as pbuf.
  // DTR (bit 0 of wValue) is set when the host opens the port and cleared
  // when it closes it. usb_ready is set before connect_cb is dispatched
  // so the callback can transmit a greeting.
  if (cmd == CDC_SET_CONTROL_LINE_STATE) {
    USBD_SetupReqTypedef *req = (USBD_SetupReqTypedef *)pbuf;
    bool dtr = (req->wValue & 0x01) != 0;
    if (dtr && !cdc_device->usb_ready) {
      cdc_device->usb_ready = true;
      schedule_now(connect_task, cdc_device);
    } else if (!dtr && cdc_device->usb_ready) {
      cdc_device->usb_ready = false;
      schedule_now(disconnect_task, cdc_device);
    }
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

// Task to deliver TX complete to application callback (can't call from ISR)
static void tx_complete_task(void *data) {
  usbd_cdc_state_t *cdc = (usbd_cdc_state_t *)data;

  // Mark TX as complete just before invoking callback, so tx_ready()
  // returns true exactly when the callback runs (not before)
  cdc->tx_busy = false;
  cdc->tx_buf_in_flight = NULL;

  if (cdc->tx_complete_cb) {
    cdc->tx_complete_cb(cdc, cdc->user_data);
  }
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

  // Notify application via scheduler (not from ISR context)
  // Note: tx_busy is cleared in tx_complete_task, not here, so that
  // tx_ready() returns false until the callback actually runs
  schedule_now(tx_complete_task, cdc_device);

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

  // Enable HSI48 oscillator (48 MHz for USB). On G0/G4 the HSI48
  // output feeds the USB peripheral clock directly; no separate
  // peripheral-clock-source selector is needed.
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
#elif defined(RULOS_ARM_stm32h5)
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};

  // Enable HSI48 oscillator (48 MHz for USB). On H5 this does NOT
  // automatically route to the USB peripheral -- see the peripheral
  // clock source selection below.
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    __builtin_trap();
  }

  // Route HSI48 to the USB peripheral clock selector. On H5 the
  // USB peripheral has its own clock mux (selectable between HSI48,
  // PLL3Q, etc.) that must be programmed via the peripheral-clock
  // init API, not via the bare RCC_OscInitStruct.
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    __builtin_trap();
  }

  // H5 has an isolated USB power domain (VDDUSB) that must be
  // explicitly switched on before the USB_DRD_FS peripheral can be
  // used. Easy to miss -- the bare `__HAL_RCC_USB_CLK_ENABLE()` in
  // MspInit is not sufficient on H5 where it is on G4.
  HAL_PWREx_EnableVddUSB();

  // Enable CRS (Clock Recovery System) for HSI48. The sequence is
  // the same as G0/G4: use USB SOF as the sync source to trim the
  // internal 48 MHz oscillator to the host's reference.
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
// RULOS composite descriptor: stock CDC + a DFU 1.1 runtime interface
//
// Whether this product also exposes DFU is RULOS policy, not an ST
// library concern, so the vendored CDC tree stays pristine. At init we
// take ST's own CDC configuration descriptor verbatim, insert an
// Interface Association Descriptor that groups the two CDC interfaces
// as one function, append a RULOS-owned DFU runtime interface, and fix
// the interface count and total length. The DFU interface has no
// endpoints -- its only request, DFU_DETACH, rides EP0 and is handled
// in CDC_Control_FS -- so no CDC class behaviour changes; only the
// three GetXxxConfigDescriptor getters are overridden. The device
// descriptor's class triplet is EF/02/01 (usbd_desc.c) so the host
// looks for the IAD.
//////////////////////////////////////////////////////////////////////////////

// 8-byte Interface Association Descriptor grouping the two CDC
// interfaces (0 and 1) as one function. With three interfaces of two
// different classes in one configuration, the host needs the IAD to
// know interfaces 0-1 are the CDC-ACM function and interface 2 is
// separate; without it Linux cdc-acm binds but does not auto-assert
// DTR on open (so a bare `cat` sees nothing), and Windows mis-binds.
// It is placed immediately before interface 0, per the USB spec.
#define RULOS_IAD_DESC_LEN 8
static const uint8_t rulos_cdc_iad[RULOS_IAD_DESC_LEN] = {
    0x08, 0x0B,  // bLength, bDescriptorType: INTERFACE ASSOCIATION
    0x00,        // bFirstInterface: 0
    0x02,        // bInterfaceCount: 2 (CDC control + CDC data)
    0x02,        // bFunctionClass: CDC Communications
    0x02,        // bFunctionSubClass: Abstract Control Model
    0x01,        // bFunctionProtocol: AT commands
    0x00,        // iFunction
};

// 9-byte DFU interface descriptor + 9-byte DFU 1.1 functional descriptor.
#define RULOS_DFU_DESC_LEN 18
static const uint8_t rulos_dfu_desc[RULOS_DFU_DESC_LEN] = {
    // Interface #2, no endpoints, class 0xFE (Application Specific) /
    // subclass 1 (DFU) / protocol 1 (runtime, not DFU mode).
    0x09, USB_DESC_TYPE_INTERFACE, 0x02, 0x00, 0x00, 0xFE, 0x01, 0x01, 0x00,
    // DFU functional descriptor (DFU 1.1).
    0x09, 0x21,
    0x0D,        // bmAttributes: WillDetach|ManifestTolerant|CanDnload
    0xE8, 0x03,  // wDetachTimeOut: 1000 ms
    0x00, 0x04,  // wTransferSize: 1024
    0x10, 0x01,  // bcdDFUVersion: 1.1
};

// Stock cfg descriptor + the inserted IAD + the appended DFU block,
// one buffer per speed the ST core may request.
#define RULOS_CFG_BUFLEN \
  (USB_CDC_CONFIG_DESC_SIZ + RULOS_IAD_DESC_LEN + RULOS_DFU_DESC_LEN)
static uint8_t rulos_cfg_fs[RULOS_CFG_BUFLEN];
static uint8_t rulos_cfg_hs[RULOS_CFG_BUFLEN];
static uint8_t rulos_cfg_os[RULOS_CFG_BUFLEN];
static uint16_t rulos_fs_len, rulos_hs_len, rulos_os_len;

// The configuration descriptor is a 9-byte header followed by the
// interface/endpoint descriptors.
#define USB_CONFIG_HDR_LEN 9

// Build the composite: stock config header, then the IAD, then the
// stock interfaces (CDC 0+1), then the DFU interface. Patch
// wTotalLength and bNumInterfaces. Returns the composite length.
static uint16_t build_composite(const uint8_t *src, uint16_t srclen,
                                uint8_t *dst) {
  uint16_t total = (uint16_t)(srclen + RULOS_IAD_DESC_LEN +
                              RULOS_DFU_DESC_LEN);
  memcpy(dst, src, USB_CONFIG_HDR_LEN);
  memcpy(dst + USB_CONFIG_HDR_LEN, rulos_cdc_iad, RULOS_IAD_DESC_LEN);
  memcpy(dst + USB_CONFIG_HDR_LEN + RULOS_IAD_DESC_LEN,
         src + USB_CONFIG_HDR_LEN, srclen - USB_CONFIG_HDR_LEN);
  memcpy(dst + RULOS_IAD_DESC_LEN + srclen, rulos_dfu_desc,
         RULOS_DFU_DESC_LEN);
  dst[2] = (uint8_t)(total & 0xFF);  // wTotalLength low
  dst[3] = (uint8_t)(total >> 8);    // wTotalLength high
  dst[4] += 1;  // bNumInterfaces += the DFU interface (IAD is not one)
  return total;
}

static uint8_t *rulos_get_fs_cfg(uint16_t *length) {
  *length = rulos_fs_len;
  return rulos_cfg_fs;
}
static uint8_t *rulos_get_hs_cfg(uint16_t *length) {
  *length = rulos_hs_len;
  return rulos_cfg_hs;
}
static uint8_t *rulos_get_os_cfg(uint16_t *length) {
  *length = rulos_os_len;
  return rulos_cfg_os;
}

// Stock CDC class with only the three config-descriptor getters
// replaced by the composite ones; built once in usbd_cdc_init().
static USBD_ClassTypeDef rulos_cdc_dfu_class;

static void build_rulos_cdc_dfu_class(void) {
  uint16_t len;
  uint8_t *p;

  p = USBD_CDC.GetFSConfigDescriptor(&len);
  rulos_fs_len = build_composite(p, len, rulos_cfg_fs);
  p = USBD_CDC.GetHSConfigDescriptor(&len);
  rulos_hs_len = build_composite(p, len, rulos_cfg_hs);
  p = USBD_CDC.GetOtherSpeedConfigDescriptor(&len);
  rulos_os_len = build_composite(p, len, rulos_cfg_os);

  rulos_cdc_dfu_class = USBD_CDC;  // inherit all stock CDC behaviour
  rulos_cdc_dfu_class.GetFSConfigDescriptor = rulos_get_fs_cfg;
  rulos_cdc_dfu_class.GetHSConfigDescriptor = rulos_get_hs_cfg;
  rulos_cdc_dfu_class.GetOtherSpeedConfigDescriptor = rulos_get_os_cfg;
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

  // Register the stock CDC class augmented with the RULOS DFU runtime
  // interface (composite descriptor; vendored CDC code untouched).
  build_rulos_cdc_dfu_class();
  if (USBD_RegisterClass(&cdc->usbd_handle, &rulos_cdc_dfu_class) != USBD_OK) {
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

int usbd_cdc_print(usbd_cdc_state_t *cdc, const char *s) {
  return usbd_cdc_write(cdc, s, strlen(s));
}
