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

#include "max3421e.h"

#include <stdint.h>

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/spi/hal_spi.h"
#include "usbstructs.h"

// Set transfer bounds
#define NAK_LIMIT                3
#define TIMEOUT_LIMIT            3
#define USB_BUS_PROBE_PERIOD_MS  500
#define USB_SETTLE_DELAY_MS      200
#define USB_RESET_WAIT_PERIOD_MS 50

// USB 2.0 section 9.2.6.3 requires a 2msec delay after setting an address but
// apparently the older spec requires at least a 200msec wait? The USB Host
// Shield waits 300 msec here. I don't know....
#define USB_POST_ADDRESS_WAIT_MS 300

// Debug options
#define PRINT_DEVICE_INFO 1
#define VERBOSE_LOGGING   0

/////////////////////////////////////////////////////////////////////

#if VERBOSE_LOGGING
#define VLOG LOG
#else
#define VLOG(...)
#endif

// Write multiple values to a MAX3421E register. "reg" is assumed to be the
// register number shifted-left by 3.
static void multi_write_reg(uint8_t reg_shifted, uint8_t *buf, uint16_t len) {
  // Set bit 1, which indicates this is a write (not a read)
  reg_shifted |= 0x2;

  hal_spi_select_slave(TRUE);

  // transmit register number
  hal_spi_send(reg_shifted);

  // transmit the bulk data
  hal_spi_send_multi(buf, len);
  hal_spi_select_slave(FALSE);
}

// Write a value to a MAX3421E register. "reg" is assumed to be the register
// number shifted-left by 3.
static void write_reg(uint8_t reg_shifted, uint8_t val) {
  multi_write_reg(reg_shifted, &val, 1);
}

// Read a value from a MAX3421E register. "reg" is assumed to be the register
// number shifted-left by 3.
static uint8_t read_reg(uint8_t reg_shifted) {
  uint8_t bufOut[2], bufIn[2];
  bufOut[0] = reg_shifted;
  bufOut[1] = 0;
  hal_spi_select_slave(TRUE);
  hal_spi_sendrecv_multi(bufOut, bufIn, 2);
  hal_spi_select_slave(FALSE);
  return bufIn[1];
}

usb_word_t host_word_to_usb(const uint16_t value) {
  usb_word_t w;
  w.high = (value >> 8) & 0xFF;
  w.low = (value & 0xFF);
  return w;
}

// Tell the max3421e to start a USB transaction with the specified
// transaction_type and endpoint. Transaction type msut be one of the tokXXX
// constants, e.g. tokSETUP for a setup packet, tokIN for an IN transaction,
// etc.
static uint8_t execute_transaction(usb_device_t *dev, usb_endpoint_t *endpoint,
                                   const uint8_t transaction_type) {
  // Set the peer address in the max3421's address register and the
  // endpoint's data toggle.
  write_reg(rPERADDR, dev->addr);
  write_reg(rHCTL, endpoint->last_rx_toggle ? bmRCVTOG1 : bmRCVTOG0);
  write_reg(rHCTL, endpoint->last_tx_toggle ? bmSNDTOG1 : bmSNDTOG0);

  uint16_t naks = 0;
  uint16_t timeouts = 0;
  while (true) {
    // Write to the transfer register to tell the max3421 to initiate the
    // transaction
    write_reg(rHXFR, transaction_type | endpoint->endpoint_addr.addr);

    // Wait for a frame to arrive
    int wait_cycles = 0;
    do {
      wait_cycles++;
    } while (!(read_reg(rHIRQ) & bmHXFRDNIRQ));
    write_reg(rHIRQ, bmHXFRDNIRQ);

    // Get the result of the transaction
    uint8_t status, result;
    do {
      wait_cycles++;
      status = read_reg(rHRSL);
      result = status & 0xF;
    } while (result == hrBUSY);

    endpoint->last_rx_toggle = (status & bmRCVTOGRD) ? 1 : 0;
    endpoint->last_tx_toggle = (status & bmSNDTOGRD) ? 1 : 0;

    VLOG("transaction executed after %d cycles, result=%d", wait_cycles,
         result);

    switch (result) {
      case hrNAK:
        // In the case of a NAK, retry unless we've hit the NAK limit. Once we
        // hit the limit, return a hard error.
        if (++naks > NAK_LIMIT) {
          return result;
        }
        break;

      case hrTIMEOUT:
        // In the case of a timeout, retry unless we've hit the timeout
        // limit. Once we hit the limit, return a hard error.
        if (++timeouts > TIMEOUT_LIMIT) {
          return result;
        }
        break;

      default:
        // In all other cases -- success or failure -- return the result code.
        return result;
    }
  }
}

uint8_t max3421e_read_data(usb_device_t *dev, usb_endpoint_t *endpoint,
                           uint8_t *result_buf, uint16_t max_result_len,
                           uint16_t *result_len_received) {
  assert(endpoint->max_packet_len > 0);
  assert(max_result_len > 0);

  uint16_t total_bytes_read = 0;
  while (true) {
    uint8_t result = execute_transaction(dev, endpoint, tokIN);

    VLOG("reading data: result=%d", result);
    if (result) {
      return result;
    }
    uint8_t bytes_read = read_reg(rRCVBC);
    VLOG("....reading %d bytes", bytes_read);

    if (bytes_read > max_result_len) {
      LOG("USB: read %d bytes, then got more bytes (%d) than we expected (%d)!",
          total_bytes_read, bytes_read, max_result_len);
      bytes_read = max_result_len;
    }

    for (int i = 0; i < bytes_read; i++) {
      *result_buf++ = read_reg(rRCVFIFO);
      total_bytes_read++;
      max_result_len--;
    }
    write_reg(rHIRQ, bmRCVDAVIRQ);

    // The transfer is complete either after receiving a short packet or we've
    // read all data we expect.
    if (bytes_read < endpoint->max_packet_len || max_result_len == 0) {
      *result_len_received = total_bytes_read;
      return 0;
    }
  }
}

// Perform a control request to the specified device and endpoint. |setup| is
// assumed to be filled in with the desired control request with the exception
// of the wLength field, which this function fills in to match |data_length|.
// Return value is 0 if successful, an error code otherwise.
static uint8_t control_common(usb_device_t *dev, usb_endpoint_t *endpoint,
                              USB_SETUP_PACKET *setup,
                              const uint8_t data_length) {
  // Convert the data_length field and write it to the setup packet.
  setup->wLength = host_word_to_usb(data_length);

  // Write the setup packet to the max3421e's control register.
  multi_write_reg(rSUDFIFO, (uint8_t *)setup, sizeof(USB_SETUP_PACKET));

  // Set the data toggle to 1, which (I think!?) is always the data toggle value
  // for control requests.
  endpoint->last_rx_toggle = 1;

  // Send the setup packet. Return if we get an error.
  VLOG("sending setup packet");
  return execute_transaction(dev, endpoint, tokSETUP);
}

// Execute a control request that has a data reading phase to the specified
// device. |setup| is assumed to be filled in with all fields other than length,
// which is filled automatically from |max_result_len|. The data read is written
// to |resultbuf|. |result_len_received| indicates how much data was
// written. Returns zero on success or a non-zero error code in case of failure.
static uint8_t control_read(usb_device_t *dev, USB_SETUP_PACKET *setup,
                            uint8_t *resultbuf, const uint16_t max_result_len,
                            uint16_t *result_len_received) {
  // The control endpoint is always endpoint 0.
  usb_endpoint_t *control_endpoint = &dev->endpoints[0];

  uint8_t result = control_common(dev, control_endpoint, setup, max_result_len);
  if (result) {
    return result;
  }

  result = max3421e_read_data(dev, control_endpoint, resultbuf, max_result_len,
                              result_len_received);

  if (result) {
    return result;
  }

  VLOG("sending ack");
  return execute_transaction(dev, control_endpoint, tokOUTHS);
}

// Execute a control request that has an optional data writing phase to the
// specified device. |setup| is assumed to be filled in with all fields other
// than length, which is filled automatically from |len|. Returns zero on
// success or a non-zero error code in case of failure.
//
// NB: Currently does not support a data phase.
static uint8_t control_write(usb_device_t *dev, USB_SETUP_PACKET *setup,
                             uint8_t *data, const uint16_t len) {
  // TODO: Support control writes with a data phase.
  assert(len == 0);

  // The control endpoint is always endpoint 0.
  usb_endpoint_t *control_endpoint = &dev->endpoints[0];

  uint8_t result = control_common(dev, control_endpoint, setup, len);
  if (result) {
    return result;
  }

  if (len == 0) {
    // With no data stage, the last operation is to send an IN token to the
    // peripheral as the STATUS (handshake) stage of this control transfer. We
    // should get NAK or the DATA1 PID. When we get the DATA1 PID, the max3421
    // automatically sends the closing ACK.
    result = execute_transaction(dev, control_endpoint, tokINHS);
  }

  return result;
}

// Configures the device at |dev| to have new address |addr|. If successful, 0
// is returned and dev->addr is updated to be addr. On failure, a non-zero
// result is returned and dev->addr is not changed.
static uint8_t configure_device_address(max3421e_t *max, usb_device_t *dev,
                                        const uint8_t addr) {
  VLOG("Reconfiguring new device with address %d", addr);

  USB_SETUP_PACKET setup = {};
  setup.ReqType_u.bmRequestType = bmREQ_SET;
  setup.bRequest = USB_REQUEST_SET_ADDRESS;
  setup.wValue.low = addr;
  uint8_t result = control_write(dev, &setup, NULL, 0);

  // If the request succeeded, update our record of this device's address so
  // that future requests go to the right place.
  if (result == 0) {
    dev->addr = addr;
  }
  return result;
}

static uint8_t activate_configuration(max3421e_t *max, usb_device_t *dev,
                                      const uint8_t config_number) {
  VLOG("Device at address %d, activating configuration %d", dev->addr,
       config_number);

  USB_SETUP_PACKET setup = {};
  setup.ReqType_u.bmRequestType = bmREQ_SET;
  setup.bRequest = USB_REQUEST_SET_CONFIGURATION;
  setup.wValue.low = config_number;
  return control_write(dev, &setup, NULL, 0);
}

// Get a device descriptor from the device at |dev|, written to |udd|. Return
// value is 0 if successful, an error code otherwise.
static uint8_t get_device_descriptor(max3421e_t *max, usb_device_t *dev,
                                     USB_DEVICE_DESCRIPTOR *udd) {
  USB_SETUP_PACKET setup = {};
  setup.ReqType_u.bmRequestType = bmREQ_GET_DESCR;
  setup.bRequest = USB_REQUEST_GET_DESCRIPTOR;
  setup.wValue.high = USB_DESCRIPTOR_DEVICE;

  // First, read the first 8 bytes of the device descriptor, which is enough to
  // give us the max packet len for endpoint 0. This initial read assumes the
  // max_packet_len is 8 until we find out the real value from the USB device
  // descriptor.
  dev->endpoints[0].max_packet_len = 8;
  uint16_t received_len = 0;
  uint8_t result = control_read(dev, &setup, (uint8_t *)udd,
                                /* max_result_len=*/8, &received_len);
  if (result) {
    return result;
  }

  if (udd->bMaxPacketSize0 == 0) {
    LOG("USB: invalid max packet len %d!", udd->bMaxPacketSize0);
    return hrINVALID;
  }

  dev->endpoints[0].max_packet_len = udd->bMaxPacketSize0;

  // Execute the read again using the now-known max-packet-len for the device.
  result = control_read(dev, &setup, (uint8_t *)udd,
                        sizeof(USB_DEVICE_DESCRIPTOR), &received_len);

  if (result) {
    return result;
  }

  if (received_len != sizeof(USB_DEVICE_DESCRIPTOR)) {
    return hrSHORTPACKET;
  }

  if (udd->bDescriptorType != USB_DESCRIPTOR_DEVICE) {
    LOG("USB: Got unexpected dev descriptor type 0x%x", udd->bDescriptorType);
    return hrSHORTPACKET;
  }

  return 0;
}

// Get a config descriptor from the device at |dev|, written to |ucd|. Return
// value is 0 if successful, an error code otherwise.
static uint8_t get_config_descriptor(max3421e_t *max, usb_device_t *dev,
                                     const uint8_t config_number,
                                     uint8_t *resultbuf,
                                     const uint16_t max_result_len,
                                     uint16_t *result_len_received) {
  USB_SETUP_PACKET setup = {};
  setup.ReqType_u.bmRequestType = bmREQ_GET_DESCR;
  setup.bRequest = USB_REQUEST_GET_DESCRIPTOR;
  setup.wValue.high = USB_DESCRIPTOR_CONFIGURATION;
  setup.wValue.low = config_number;

  uint8_t result =
      control_read(dev, &setup, resultbuf, max_result_len, result_len_received);

  if (result) {
    return result;
  }

  if (*result_len_received < sizeof(USB_CONFIGURATION_DESCRIPTOR)) {
    return hrSHORTPACKET;
  }

  return 0;
}

uint8_t max3421e_set_hid_idle(usb_device_t *dev, usb_endpoint_t *endpoint,
                              const uint8_t idle_rate) {
  USB_SETUP_PACKET setup = {};
  setup.ReqType_u.bmRequestType = bmREQ_HID_OUT;
  setup.bRequest = HID_REQUEST_SET_IDLE;
  setup.wValue.low = 0;
  setup.wValue.high = idle_rate;
  setup.wIndex = host_word_to_usb(endpoint->interface_id);
  return control_write(dev, &setup, NULL, 0);
}

// Get the primary language from the USB string descriptor set. On success,
// returns 0 and sets primary_language. On failure, returns a non-zero error
// code and does not touch primary_language.
static uint8_t get_primary_language(max3421e_t *max, usb_device_t *dev,
                                    usb_word_t *primary_language) {
  USB_SETUP_PACKET setup = {};
  setup.ReqType_u.bmRequestType = bmREQ_GET_DESCR;
  setup.bRequest = USB_REQUEST_GET_DESCRIPTOR;
  setup.wValue.high = USB_DESCRIPTOR_STRING;

  // Ensure the USB buffer is big enough for a descriptor containing at least 1
  // language
  assert(USB_BUFSIZE > sizeof(USB_LANG_DESCRIPTOR) + sizeof(usb_word_t));

  // Retrieve the language descriptor
  USB_LANG_DESCRIPTOR *const uld = (USB_LANG_DESCRIPTOR *)max->xferbuf;
  uint16_t received_len;
  uint8_t result = control_read(dev, &setup, max->xferbuf, sizeof(max->xferbuf),
                                &received_len);

  if (result) {
    return result;
  }

  // Make sure we got at least a header
  if (received_len < sizeof(USB_LANG_DESCRIPTOR)) {
    LOG("USB: got short lang descriptor of only %d bytes!", received_len);
    return hrSHORTPACKET;
  }

  // Count how many languages were returned
  const uint8_t num_languages =
      (received_len - sizeof(USB_LANG_DESCRIPTOR)) / sizeof(usb_word_t);

  if (num_languages == 0) {
    LOG("USB: no languages returned!");
    return hrSHORTPACKET;
  }

  VLOG("got %d languages; lang 0 is 0x%02x%02x", num_languages,
       uld->LANGID[0].high, uld->LANGID[0].low);
  *primary_language = uld->LANGID[0];
  return 0;
}

static uint8_t get_string_descriptor(max3421e_t *max, usb_device_t *dev,
                                     const uint8_t string_index,
                                     const usb_word_t language, char *buf,
                                     uint16_t buflen) {
  USB_SETUP_PACKET setup = {};
  setup.ReqType_u.bmRequestType = bmREQ_GET_DESCR;
  setup.bRequest = USB_REQUEST_GET_DESCRIPTOR;
  setup.wValue.high = USB_DESCRIPTOR_STRING;
  setup.wValue.low = string_index;
  setup.wIndex = language;

  uint16_t received_len = 0;
  uint8_t result =
      control_read(dev, &setup, (uint8_t *)buf, buflen - 1, &received_len);

  // Do a bodged conversion from utf16 (which is what USB returns) to ASCII.
  char *converted = buf;
  for (int i = 2; i < received_len; i += 2) {
    *converted++ = buf[i];
  }
  *converted = '\0';
  return result;
}

static void print_one_device_string(max3421e_t *max, usb_device_t *dev,
                                    const char *prefix,
                                    USB_DEVICE_DESCRIPTOR *udd,
                                    const uint8_t index,
                                    const usb_word_t language) {
  if (index == 0) {
    return;
  }

  char buf[100];
  uint8_t result =
      get_string_descriptor(max, dev, index, language, buf, sizeof(buf));
  if (result) {
    LOG("USB: error getting string: %d", result);
  } else {
    LOG("USB: %s: %s", prefix, buf);
  }
}

static void print_device_info(max3421e_t *max, usb_device_t *dev,
                              USB_DEVICE_DESCRIPTOR *udd) {
  usb_word_t primary_language;
  uint8_t result = get_primary_language(max, dev, &primary_language);

  if (result) {
    LOG("USB: error getting language list: %d", result);
    return;
  }

  print_one_device_string(max, dev, "Manufacturer", udd, udd->iManufacturer,
                          primary_language);
  print_one_device_string(max, dev, "Device", udd, udd->iProduct,
                          primary_language);
}

// Attempt to reset the max3421. Returns true if it's been reset successfully;
// false if it has not.
static bool max_reset() {
  // Assert and then release software reset bit
  write_reg(rUSBCTL, bmCHIPRES);
  write_reg(rUSBCTL, 0);

  // At 64mhz this consistently takes 68 iterations. TODO: make this
  // time-based rather than a counter.
  const int max_wait = 500;
  int i = 0;

  while (i++ < max_wait) {
    if (read_reg(rUSBIRQ) & bmOSCOKIRQ) {
      VLOG("reset took %d iterations", i);
      return true;
    }
  }

  return false;
}

void step1_probe_bus(void *data);
void initiate_periph_config(max3421e_t *max, uint8_t is_lowspeed);
void step2_reset_device(void *data);
void step3_enable_framemarker(void *data);
void step4_delay_after_first_frameirq(void *data);
void step5_configure_address(void *data);
void step6_get_metadata(void *data);

// Resets internal state both when USB starts and when a disconnect occurs.
static void reset_state(max3421e_t *max) {
  memset(max->devices, 0, sizeof(max->devices));
  max->connected = false;
}

// Bus probe step 1: check to see if anything new is attached to the bus.
void step1_probe_bus(void *data) {
  max3421e_t *max = (max3421e_t *)data;

  VLOG("probing bus");

  // Probe bus to see if anything is attached. We sometimes see spurious
  // disconnects for some reason, so we keep sampling the bus until we get 3 of
  // the same value in a row.
  //
  // NB: The docs say that you're supposed to wait for bmSAMPLEBUS to be cleared
  // to indicate sampling is complete before reading the status registers. The
  // max3421e I have never seems to clear the bit, no matter how long I
  // wait. The spurious disconnects might be due to a race if I'm reading the
  // status register before bus sampling is complete.
  uint8_t num_consistent_samples = 0;
  uint8_t bus_state = 0xFF;
  uint8_t is_lowspeed = 0;
  do {
    write_reg(rHCTL, bmSAMPLEBUS);
    uint8_t sample = read_reg(rHRSL) & (bmJSTATUS | bmKSTATUS);
    if (bus_state == sample) {
      num_consistent_samples++;
    } else {
      bus_state = sample;
      num_consistent_samples = 0;
    }
  } while (num_consistent_samples < 3);

  // If nothing has changed since our last probe, just reschedule another bus
  // probe for later.
  if (max->last_bus_state == bus_state) {
    goto probe_again_later;
  }

  // If we're in an illegal state, report an error and try again later.
  if (bus_state == (bmJSTATUS | bmKSTATUS)) {
    LOG("USB Warning: bus seems to be in an illegal state!");
    goto probe_again_later;
  }

  max->last_bus_state = bus_state;
  is_lowspeed = read_reg(rMODE) & bmLOWSPEED;

  switch (bus_state) {
    case 0:
      // If both lines are low, the device has disconnected.
      LOG("USB: Disconnect detected");
      max->connected = false;
      reset_state(max);
      goto probe_again_later;

    case bmJSTATUS:
      // Idle state detected. If we're already connected, do nothing.
      if (max->connected) {
        goto probe_again_later;
      }

      // A device just connected with the "correct polarity" -- meaning the
      // low-speed bit is already in the right position. Initiate a scan using
      // the current speed setting.
      initiate_periph_config(max, is_lowspeed);
      return;

    case bmKSTATUS:
      if (max->connected) {
        LOG("USB: Got K status while already connected!?");
        goto probe_again_later;
      } else {
        // If we're not connected and we see a K status it means we have to flip
        // the lowspeed bit.
        initiate_periph_config(max, !is_lowspeed);
        return;
      }

    default:
      assert(false);
  }

probe_again_later:
  schedule_us(USB_BUS_PROBE_PERIOD_MS * 1000, step1_probe_bus, max);
  return;
}

void initiate_periph_config(max3421e_t *max, uint8_t is_lowspeed) {
  uint8_t new_mode = bmDPPULLDN | bmDMPULLDN | bmHOST | bmSOFKAENAB;

  if (is_lowspeed) {
    new_mode |= bmLOWSPEED;
    LOG("USB: Low-Speed Device Detected");
  } else {
    LOG("USB: Full-Speed Device Detected");
  }

  max->connected = true;
  write_reg(rMODE, new_mode);
  schedule_us(USB_SETTLE_DELAY_MS * 1000, step2_reset_device, max);
}

// Bus probe step 2: after the device has settled, perform a bus reset to put
// the device into a known state.
void step2_reset_device(void *data) {
  max3421e_t *max = (max3421e_t *)data;

  VLOG("bus probe step2");
  write_reg(rHCTL, bmBUSRST);
  schedule_us(USB_RESET_WAIT_PERIOD_MS * 1000, step3_enable_framemarker, max);
}

// Bus probe step 3: enable frame markers, then wait for the first FRAMEIRQ.
void step3_enable_framemarker(void *data) {
  max3421e_t *max = (max3421e_t *)data;

  VLOG("bus probe step3");
  // Ensure reset has completed. If not, wait 10ms more.
  if (read_reg(rHCTL) & bmBUSRST) {
    schedule_us(10000, step3_enable_framemarker, max);
  } else {
    uint8_t mode = read_reg(rMODE);
    mode |= bmSOFKAENAB;
    write_reg(rMODE, mode);
    schedule_us(20000, step4_delay_after_first_frameirq, max);
  }
}

// Bus probe step 4: After the first frameirq has been received, wait an
// additional 20msec before configuring.
void step4_delay_after_first_frameirq(void *data) {
  max3421e_t *max = (max3421e_t *)data;

  VLOG("bus probe step4");
  if (read_reg(rHIRQ) & bmFRAMEIRQ) {
    schedule_us(20000, step5_configure_address, max);
  } else {
    schedule_us(20000, step4_delay_after_first_frameirq, max);
  }
}

// Bus probe step 5: Now that the bus traffic is flowing, query the new device
// for its parameters, configure an address, etc.
void step5_configure_address(void *data) {
  max3421e_t *max = (max3421e_t *)data;

  VLOG("bus probe step5");

  // Find an empty slot in the USB array
  usb_device_t *dev = NULL;
  uint8_t addr;

  for (int i = 0; i < MAX_USB_DEVICES; i++) {
    if (max->devices[i].addr == 0) {
      dev = &max->devices[i];
      addr = i + ADDR_OFFSET;
      break;
    }
  }

  // Make sure a slot was available!
  if (dev == NULL) {
    LOG("out of USB device slots!");
    assert(FALSE);
  }

  // Initialize device.
  dev->num_endpoints = 1;  // configuration endpoint is always endpoint #0

  // Reconfigure the address of the new device. This updates the address field
  // of 'dev'.
  uint8_t result = configure_device_address(max, dev, addr);
  if (result) {
    LOG("couldn't configure device address: %d", result);
  }

  schedule_us(USB_POST_ADDRESS_WAIT_MS * 1000, step6_get_metadata, max);
}

static void parse_endpoint(usb_device_t *dev, const uint8_t interface_id,
                           USB_ENDPOINT_DESCRIPTOR *ued) {
  if (dev->num_endpoints >= MAX_USB_ENDPOINTS) {
    LOG("USB error: more endpoints in this device than we can store");
    return;
  }

  usb_endpoint_t *endpoint = &dev->endpoints[dev->num_endpoints++];
  endpoint->interface_id = interface_id;
  endpoint->endpoint_addr = ued->bEndpointAddress;
  endpoint->max_packet_len = ued->wMaxPacketSize;

  LOG("USB: device %d: interface %d has endpoint #%d at addr 0x%x (%s), "
      "max packet len %d",
      dev->addr, interface_id, dev->num_endpoints - 1,
      endpoint->endpoint_addr.addr,
      endpoint->endpoint_addr.direction ? "IN" : "OUT",
      endpoint->max_packet_len);
}

static void get_metadata_one_device(max3421e_t *max, usb_device_t *dev) {
  USB_DEVICE_DESCRIPTOR udd = {};
  uint8_t result = get_device_descriptor(max, dev, &udd);
  if (result) {
    LOG("error getting usb device descriptor: %d", result);
    return;
  }

  dev->vid = udd.idVendor;
  dev->pid = udd.idProduct;
  LOG("USB: Got device descriptor! vid=0x%x, pid=0x%x", dev->vid, dev->pid);

#if PRINT_DEVICE_INFO
  print_device_info(max, dev, &udd);
#endif

  // Make sure the number of configurations is what we expect
  if (udd.bNumConfigurations == 0) {
    LOG("USB error: no configurations available!?");
    return;
  }

  if (udd.bNumConfigurations > 1) {
    LOG("USB warning: this device has %d configurations; using the first",
        udd.bNumConfigurations);
  }

  // Get the configuration descriptor; all interface and endpoint
  // descriptors come with it.
  uint16_t received_len;
  result = get_config_descriptor(max, dev, 0, max->xferbuf,
                                 sizeof(max->xferbuf), &received_len);
  if (result) {
    LOG("Couldn't get config descriptor: %d", result);
    return;
  }

  // Parse each block of the configuration descriptor.
  uint8_t *next_to_parse = max->xferbuf;
  USB_CONFIGURATION_DESCRIPTOR *ucd =
      (USB_CONFIGURATION_DESCRIPTOR *)next_to_parse;
  next_to_parse += sizeof(USB_CONFIGURATION_DESCRIPTOR);

  // Check for sanity.
  if (ucd->bLength != sizeof(USB_CONFIGURATION_DESCRIPTOR) ||
      ucd->bDescriptorType != USB_DESCRIPTOR_CONFIGURATION) {
    LOG("USB fatal: config descriptor had unexpected len %d, type 0x%x",
        ucd->bLength, ucd->bDescriptorType);
    return;
  }

  // The descriptor tells us how long it's supposed to be in the early part of
  // the header; make sure our transfer buffer is long enough to get the whole
  // thing.
  if (ucd->wTotalLength > sizeof(max->xferbuf)) {
    LOG("USB fatal: USB xferbuf is only %d bytes, needs to be at least %d",
        sizeof(max->xferbuf), ucd->wTotalLength);
    return;
  }
  const uint8_t *descriptor_end = &max->xferbuf[ucd->wTotalLength];

  // Loop through all interfaces and endpoints in this configuration. Note that,
  // at least for the time being, we will ignore interface descriptors and just
  // compile a list of endpoints (and their associated interface numbers).
  while (next_to_parse < descriptor_end) {
    uint8_t curr_interface_id = 0;
    USB_DESCRIPTOR_HEADER *udh = (USB_DESCRIPTOR_HEADER *)next_to_parse;
    next_to_parse += udh->bLength;

    VLOG("got a descriptor of type 0x%x", udh->bDescriptorType);

    switch (udh->bDescriptorType) {
      case USB_DESCRIPTOR_INTERFACE: {
        USB_INTERFACE_DESCRIPTOR *uid = (USB_INTERFACE_DESCRIPTOR *)udh;
        curr_interface_id = uid->bInterfaceNumber;
        break;
      }

      case USB_DESCRIPTOR_ENDPOINT: {
        parse_endpoint(dev, curr_interface_id, (USB_ENDPOINT_DESCRIPTOR *)udh);
        break;
      }

      default: {
        VLOG("not parsing usb config descriptor type 0x%x",
             udh->bDescriptorType);
        break;
      }
    }
  }

  // Configure device to use the first configuration
  result = activate_configuration(max, dev, ucd->bConfigurationValue);
  if (result) {
    LOG("couldn't activate configuration: %d", result);
    return;
  }

  // Indicate to potential clients that the device is ready
  dev->ready = true;
}

// Bus probe step 6: Get metadata from just-configured devices, which we can
// identify because the vid/pid are 0.
void step6_get_metadata(void *data) {
  max3421e_t *max = (max3421e_t *)data;

  VLOG("bus probe step6");

  for (int i = 0; i < MAX_USB_DEVICES; i++) {
    if (max->devices[i].addr != 0 && max->devices[i].vid == 0) {
      get_metadata_one_device(max, &max->devices[i]);
    }
  }

  // Now that the device address has been configured, we can also restart bus
  // probing without seeing the same device again. Or, if address setting
  // failed, bus probe will cause us to try again. Either way, we are ready to
  // probe the bus again.
  schedule_us(USB_BUS_PROBE_PERIOD_MS * 1000, step1_probe_bus, max);
}

bool max3421e_init(max3421e_t *max) {
  reset_state(max);
  hal_init_spi();

  // Set full duplex SPI mode
  write_reg(rPINCTL, bmFDUPSPI);

  // Attempt reset
  if (!max_reset()) {
    LOG("USB: MAX3421e reset failed");
    return false;
  }

  max->chip_ver = read_reg(rREVISION);
  LOG("USB: MAX3421e online, revision %d", max->chip_ver);

  // Activate host mode and turn on the pulldown resistors on D+ and D-
  write_reg(rMODE, bmDPPULLDN | bmDMPULLDN | bmHOST);

  schedule_us(1, step1_probe_bus, max);
  return true;
}
