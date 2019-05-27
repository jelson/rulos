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
#include "usbstructs.h"

#include "periph/spi/hal_spi.h"

// Set transfer bounds
#define NAK_LIMIT 200
#define TIMEOUT_LIMIT 3
#define USB_BUS_PROBE_PERIOD_MS 500
#define USB_SETTLE_DELAY_MS 200
#define USB_RESET_WAIT_PERIOD_MS 50

#define PRINT_DEVICE_INFO 1

/////////////////////////////////////////////////////////////////////

// Write multiple values to a MAX3421E register. "reg" is assumed to
// be the register number shifted-left by 3.
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

// Write a value to a MAX3421E register. "reg" is assumed to be the
// register number shifted-left by 3.
static void write_reg(uint8_t reg_shifted, uint8_t val) {
  multi_write_reg(reg_shifted, &val, 1);
}

// Read a value from a MAX3421E register. "reg" is assumed to be the
// register number shifted-left by 3.
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
// transaction_type and endpoint ID. Transaction type msut be one of the tokXXX
// constants, e.g. tokSETUP for a setup packet, tokIN for an IN transaction,
// etc.
static uint8_t execute_transaction(const uint8_t transaction_type,
                                   const uint8_t endpoint) {
  assert(endpoint == 0);

  uint16_t naks = 0;
  uint16_t timeouts = 0;
  while (true) {
    write_reg(rHXFR, transaction_type | endpoint);

    int wait_cycles = 0;
    do {
      wait_cycles++;
    } while (!(read_reg(rHIRQ) & bmHXFRDNIRQ));
    write_reg(rHIRQ, bmHXFRDNIRQ);

    uint8_t result;
    do {
      wait_cycles++;
      result = read_reg(rHRSL) & 0x0F;
    } while (result == hrBUSY);

    // LOG("transaction executed after %d cycles, result=%d", wait_cycles,
    // result);

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

static uint8_t read_data(const uint8_t endpoint, uint8_t *result_buf,
                         const uint8_t max_packet_len, uint16_t max_result_len,
                         uint16_t *result_len_received) {
  assert(max_packet_len > 0);
  assert(max_result_len > 0);

  uint16_t total_bytes_read = 0;
  while (true) {
    uint8_t result = execute_transaction(tokIN, endpoint);

    // LOG("reading data: result=%d", result);
    if (result) {
      return result;
    }
    uint8_t bytes_read = read_reg(rRCVBC);
    // LOG("....reading %d bytes", bytes_read);

    if (bytes_read > max_result_len) {
      LOG("after reading %d bytes, got more bytes (%d) than we expected (%d)!",
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
    if (bytes_read < max_packet_len || max_result_len == 0) {
      *result_len_received = total_bytes_read;
      return 0;
    }
  }
}

static uint8_t control_req(const uint8_t addr, const uint8_t endpoint,
                           USB_SETUP_PACKET *setup,
                           const uint16_t max_packet_len, uint8_t *resultbuf,
                           const uint16_t max_result_len,
                           uint16_t *result_len_received) {
  setup->wLength = host_word_to_usb(max_result_len);

  // Write the setup packet to the max3421e's control register
  multi_write_reg(rSUDFIFO, (uint8_t *)setup, sizeof(USB_SETUP_PACKET));

  write_reg(rHCTL, bmRCVTOG1);

  // Send the setup packet. Return if we get an error.
  // LOG("sending setup");
  uint8_t result = execute_transaction(tokSETUP, endpoint);
  if (result) {
    return result;
  }

  // LOG("reading data");
  result = read_data(endpoint, resultbuf, max_packet_len, max_result_len,
                     result_len_received);

  if (result) {
    return result;
  }

  // LOG("sending ack");
  return execute_transaction(tokOUTHS, endpoint);
}

static uint8_t get_device_descriptor(max3421e_t *max, const uint8_t addr,
                                     USB_DEVICE_DESCRIPTOR *udd) {
  USB_SETUP_PACKET setup = {};
  setup.ReqType_u.bmRequestType = bmREQ_GET_DESCR;
  setup.bRequest = USB_REQUEST_GET_DESCRIPTOR;
  setup.wValue.high = USB_DESCRIPTOR_DEVICE;

  // First, read the first 8 bytes of the device descriptor, which is enough to
  // give us the max packet len for endpoint 0.
  uint16_t received_len;
  uint8_t result = control_req(addr, /* endpoint=*/0, &setup,
                               /* max_packet_len=*/8, (uint8_t *)udd,
                               /* max_result_len=*/8, &received_len);
  if (result) {
    return result;
  }

  uint8_t max_packet_len = udd->bMaxPacketSize0;

  if (max_packet_len == 0) {
    LOG("invalid packet len!");
    return hrINVALID;
  }

  // Execute the read again using the now-known max-packet-len for endpoint 0.
  result =
      control_req(addr, /*endpoint=*/0, &setup, max_packet_len, (uint8_t *)udd,
                  sizeof(USB_DEVICE_DESCRIPTOR), &received_len);

  if (result) {
    return result;
  }

  if (received_len != sizeof(USB_DEVICE_DESCRIPTOR)) {
    return hrSHORTPACKET;
  }

  return 0;
}

// Get the primary language from the USB string descriptor set. On
// success, returns 0 and sets primary_language. On failure, returns a
// non-zero error code and does not touch primary_language.
static uint8_t get_primary_language(max3421e_t *max, const uint8_t addr,
                                    const uint8_t max_packet_len,
                                    usb_word_t *primary_language) {
  USB_SETUP_PACKET setup = {};
  setup.ReqType_u.bmRequestType = bmREQ_GET_DESCR;
  setup.bRequest = USB_REQUEST_GET_DESCRIPTOR;
  setup.wValue.high = USB_DESCRIPTOR_STRING;

  // Make room for at least 5 languages
  const size_t bufsize = sizeof(USB_LANG_DESCRIPTOR) + sizeof(usb_word_t) * 5;
  uint8_t langbuf[bufsize];
  USB_LANG_DESCRIPTOR *const uld = (USB_LANG_DESCRIPTOR *)&langbuf;
  uint16_t received_len;

  uint8_t result = control_req(addr, /*endpoint=*/0, &setup, max_packet_len,
                               langbuf, bufsize, &received_len);

  if (result) {
    return result;
  }

  // Make sure we got at least a header
  if (received_len < sizeof(USB_LANG_DESCRIPTOR)) {
    LOG("got short lang descriptor of only %d bytes!", received_len);
    return hrSHORTPACKET;
  }

  // Count how many languages were returned
  const uint8_t num_languages =
      (received_len - sizeof(USB_LANG_DESCRIPTOR)) / sizeof(usb_word_t);

  if (num_languages == 0) {
    LOG("no languages returned!");
    return hrSHORTPACKET;
  }

  LOG("got %d languages; lang 0 is 0x%02x%02x", num_languages,
      uld->LANGID[0].high, uld->LANGID[0].low);
  *primary_language = uld->LANGID[0];
  return 0;
}

static uint8_t get_string_descriptor(max3421e_t *max, const uint8_t addr,
                                     const uint8_t string_index,
                                     const uint8_t max_packet_len,
                                     const usb_word_t language, char *buf,
                                     uint16_t buflen) {
  USB_SETUP_PACKET setup = {};
  setup.ReqType_u.bmRequestType = bmREQ_GET_DESCR;
  setup.bRequest = USB_REQUEST_GET_DESCRIPTOR;
  setup.wValue.high = USB_DESCRIPTOR_STRING;
  setup.wIndex = language;
  setup.wValue.low = string_index;

  uint16_t received_len;
  uint8_t result = control_req(addr, /*endpoint=*/0, &setup, max_packet_len,
                               (uint8_t *)buf, buflen - 1, &received_len);

  // Do a bodged conversion from utf16 (which is what USB returns) to ASCII.
  char *converted = buf;
  for (int i = 2; i < received_len; i += 2) {
    *converted++ = buf[i];
  }
  *converted = '\0';
  return result;
}

static void print_one_device_string(max3421e_t *max, const char *prefix,
                                    const uint8_t addr,
                                    USB_DEVICE_DESCRIPTOR *udd,
                                    const uint8_t index,
                                    const usb_word_t language) {
  if (index == 0) {
    return;
  }

  char buf[100];
  uint8_t result = get_string_descriptor(max, 0, index, udd->bMaxPacketSize0,
                                         language, buf, sizeof(buf));
  if (result) {
    LOG("error getting string: %d", result);
  } else {
    LOG("%s: %s", prefix, buf);
  }
}

static void print_device_info(max3421e_t *max, const uint8_t addr,
                              USB_DEVICE_DESCRIPTOR *udd) {
  usb_word_t primary_language;
  uint8_t result =
      get_primary_language(max, addr, udd->bMaxPacketSize0, &primary_language);

  if (result) {
    LOG("error getting language list: %d", result);
    return;
  }

  print_one_device_string(max, "Manufacturer", addr, udd, udd->iManufacturer,
                          primary_language);
  print_one_device_string(max, "Device", addr, udd, udd->iProduct,
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
      LOG("reset took %d iterations", i);
      return true;
    }
  }

  return false;
}

void step1_probe_bus(void *data);
void step2_reset_device(void *data);
void step3_enable_framemarker(void *data);
void step4_delay_after_first_frameirq(void *data);
void step5_configure(void *data);

// Bus probe step 1: check
void step1_probe_bus(void *data) {
  max3421e_t *max = (max3421e_t *)max;

  // Probe bus to see if anything is attached
  write_reg(rHCTL, bmSAMPLEBUS);
  uint8_t bus_state = read_reg(rHRSL);

  if (bus_state & bmJSTATUS) {
    write_reg(rMODE, bmDPPULLDN | bmDMPULLDN | bmHOST | bmSOFKAENAB);
    LOG("Full-Speed Device Detected");
    schedule_us(USB_SETTLE_DELAY_MS * 1000, step2_reset_device, max);
  } else if (bus_state & bmKSTATUS) {
    write_reg(rMODE,
              bmDPPULLDN | bmDMPULLDN | bmHOST | bmSOFKAENAB | bmLOWSPEED);
    LOG("Low-Speed Device Detected");
    schedule_us(USB_SETTLE_DELAY_MS * 1000, step2_reset_device, max);
  } else {
    // No device was detected; just reschedule USB bus probe
    schedule_us(USB_BUS_PROBE_PERIOD_MS * 1000, step1_probe_bus, max);
  }
}

// Bus probe step 2: after the device has settled, perform a bus reset
// to put the device into a known state.
void step2_reset_device(void *data) {
  max3421e_t *max = (max3421e_t *)max;

  LOG("bus probe step2");
  write_reg(rHCTL, bmBUSRST);
  schedule_us(USB_RESET_WAIT_PERIOD_MS * 1000, step3_enable_framemarker, max);
}

// Bus probe step 3: enable frame markers, then wait for the first FRAMEIRQ.
void step3_enable_framemarker(void *data) {
  max3421e_t *max = (max3421e_t *)max;

  LOG("bus probe step3");
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
  max3421e_t *max = (max3421e_t *)max;

  LOG("bus probe step4");
  if (read_reg(rHIRQ) & bmFRAMEIRQ) {
    schedule_us(20000, step5_configure, max);
  } else {
    schedule_us(20000, step4_delay_after_first_frameirq, max);
  }
}

void step5_configure(void *data) {
  max3421e_t *max = (max3421e_t *)max;
  // schedule_now(step1_probe_bus, max);

  LOG("bus probe step5");

  // Set peer address to 0 and set initial data toggle value, which the spec
  // defines as 1
  write_reg(rPERADDR, 0);

  USB_DEVICE_DESCRIPTOR udd = {};
  uint8_t result = get_device_descriptor(max, 0, &udd);
  if (result) {
    LOG("error getting usb device descriptor: %d", result);
    return;
  }

  LOG("Got USB device descriptor! vid=0x%x, pid=0x%x", udd.idVendor,
      udd.idProduct);

  result = get_device_descriptor(max, 0, &udd);
  if (result) {
    LOG("error getting second usb device descriptor: %d", result);
    return;
  }
  LOG("got second device descriptor for fun");

#if PRINT_DEVICE_INFO
  print_device_info(max, /*addr=*/0, &udd);
#endif
}

bool max3421e_init(max3421e_t *max) {
  hal_init_spi();

  // Set full duplex SPI mode
  write_reg(rPINCTL, bmFDUPSPI);

  // Attempt reset
  if (!max_reset()) {
    LOG("MAX3421e reset failed");
    return false;
  }

  max->chip_ver = read_reg(rREVISION);
  LOG("MAX3421e online, revision %d", max->chip_ver);

  // Activate host mode and turn on the pulldown resistors on D+ and D-
  write_reg(rMODE, bmDPPULLDN | bmDMPULLDN | bmHOST);

  schedule_now(step1_probe_bus, max);
  return true;
}
