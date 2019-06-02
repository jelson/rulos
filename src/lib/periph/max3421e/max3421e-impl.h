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

#pragma once

#include <stdint.h>
#include "usbstructs.h"

// Maximum number of USB devices we can talk to simultaneously.
#ifndef MAX_USB_DEVICES
#define MAX_USB_DEVICES 2
#endif

// Maximum number of USB endpoints per device, including the control endpoint.
#ifndef MAX_USB_ENDPOINTS
#define MAX_USB_ENDPOINTS 4
#endif

#ifndef USB_BUFSIZE
#define USB_BUFSIZE 64
#endif

// The offset from the position of the device in the devices array to
// its USB address. (During configuration the address might be zero.)
#define ADDR_OFFSET 10

typedef struct {
  USB_ENDPOINT_ADDR endpoint_addr;
  uint8_t interface_id;
  uint16_t max_packet_len;

  // The USB standard dictates that every incoming and outgoing
  // packet, per-device, has a bit associated with it, called a "data
  // toggle". Each packet sent and received must alternate the value
  // of the toggle as some form of error checking. The max3421e
  // automatically toggles back and forth for consecutive transfers
  // to/from the same endpoint, but requires that the host cpu store
  // and recall the last value of the toggle when switching from one
  // endpoint to another.
  uint8_t last_rx_toggle;
  uint8_t last_tx_toggle;
} usb_endpoint_t;

typedef struct {
  // Is the device ready? Clients shouldn't access the device until this is set.
  uint8_t ready;

  // Vendor and product ID
  uint16_t vid;
  uint16_t pid;

  // USB address assigned by us to the device
  uint8_t addr;

  // Number of endpoints
  uint8_t num_endpoints;

  // Device endpoints we know about
  usb_endpoint_t endpoints[MAX_USB_ENDPOINTS];
} usb_device_t;

typedef struct {
  // revision number of the max3421
  uint8_t chip_ver;

  // bus state the last time we scanned the bus
  uint8_t last_bus_state;

  // are we already connnected?
  uint8_t connected;

  // array of all devices attached
  usb_device_t devices[MAX_USB_DEVICES];

  // transfer buffer
  uint8_t xferbuf[USB_BUFSIZE];
} max3421e_t;

// MAX3421E Registers in PERIPHERAL mode. Preshifted left three places
// to account for the 3420/21 command byte format: rrrrr0wa where 'r'
// is register number

#define rEP0FIFO (0 << 3)
#define rEP1OUTFIFO (1 << 3)
#define rEP2INFIFO (2 << 3)
#define rEP3INFIFO (3 << 3)
#define rSUDFIFO (4 << 3)
#define rEP0BC (5 << 3)
#define rEP1OUTBC (6 << 3)
#define rEP2INBC (7 << 3)
#define rEP3INBC (8 << 3)
#define rEPSTALLS (9 << 3)
// ---------bits------------
#define bmACKSTAT 0x40
#define bmSTLSTAT 0x20
#define bmSTLEP3IN 0x10
#define bmSTLEP2IN 0x08
#define bmSTLEP1OUT 0x04
#define bmSTLEP0OUT 0x02
#define bmSTLEP0IN 0x01

#define rCLRTOGS (10 << 3)
// ---------bits------------
#define bmEP3DISAB 0x80
#define bmEP2DISAB 0x40
#define bmEP1DISAB 0x20
#define bmCTGEP3IN 0x10
#define bmCTGEP2IN 0x08
#define bmCTGEP1OUT 0x04

#define rEPIRQ (11 << 3)
// ---------bits------------
#define bmSUDAVIRQ 0x20
#define bmIN3BAVIRQ 0x10
#define bmIN2BAVIRQ 0x08
#define bmOUT1DAVIRQ 0x04
#define bmOUT0DAVIRQ 0x02
#define bmIN0BAVIRQ 0x01

#define rEPIEN (12 << 3)
// ---------bits------------
#define bmSUDAVIE 0x20
#define bmIN3BAVIE 0x10
#define bmIN2BAVIE 0x08
#define bmOUT1DAVIE 0x04
#define bmOUT0DAVIE 0x02
#define bmIN0BAVIE 0x01

#define rUSBIRQ (13 << 3)
#define bmURESDNIRQ 0x80
#define bmVBUSIRQ 0x40
#define bmNOVBUSIRQ 0x20
#define bmSUSPIRQ 0x10
#define bmURESIRQ 0x08
#define bmBUSACTIRQ 0x04
#define bmRWUDNIRQ 0x02
#define bmOSCOKIRQ 0x01

#define rUSBIEN (14 << 3)
// ---------bits------------
#define bmURESDNIE 0x80
#define bmVBUSIE 0x40
#define bmNOVBUSIE 0x20
#define bmSUSPIE 0x10
#define bmURESIE 0x08
#define bmBUSACTIE 0x04
#define bmRWUDNIE 0x02
#define bmOSCOKIE 0x01

#define rUSBCTL (15 << 3)
// ---------bits------------
#define bmHOSCSTEN 0x80
#define bmVBGATE 0x40
#define bmCHIPRES 0x20
#define bmPWRDOWN 0x10
#define bmCONNECT 0x08
#define bmSIGRWU 0x04

#define rCPUCTL (16 << 3)
// ---------bits------------
#define bmIE 0x01
#define bmPUSLEWID1 0x80  // 3421 only
#define bmPULSEWID0 0x40  // 3421 only

#define rPINCTL (17 << 3)
// ---------bits------------
#define bmEP3INAK 0x80  // 3420 or 3421 peripheral mode only
#define bmEP2INAK 0x40  // 3420 or 3421 peripheral mode only
#define bmEP1INAK 0x20  // 3420 or 3421 peripheral mode only
#define bmFDUPSPI 0x10
#define bmINTLEVEL 0x08
#define bmPOSINT 0x04
#define bmGPXB 0x02
#define bmGPXA 0x01
// GPX pin selections
#define GPX_OPERATE 0x00
#define GPX_VBDET 0x01
#define GPX_BUSACT 0x02
#define GPX_SOF 0x03

#define rREVISION (18 << 3)
#define rFNADDR (19 << 3)
#define rIOPINS1 (20 << 3)
#define rGPIO (20 << 3)  // For early MAX3420E code
// ---------bits------------
#define bmGPOUT0 0x01
#define bmGPOUT1 0x02
#define bmGPOUT2 0x04
#define bmGPOUT3 0x08
#define bmGPIN0 0x10
#define bmGPIN1 0x20
#define bmGPIN2 0x40
#define bmGPIN3 0x80

#define rIOPINS2 (21 << 3)
// ---------bits------------
#define bmGPOUT4 0x01
#define bmGPOUT5 0x02
#define bmGPOUT6 0x04
#define bmGPOUT7 0x08
#define bmGPIN4 0x10
#define bmGPIN5 0x20
#define bmGPIN6 0x40
#define bmGPIN7 0x80

#define rGPINIRQ (22 << 3)
// ---------bits------------
#define bmGPINIRQ0 0x01
#define bmGPINIRQ1 0x02
#define bmGPINIRQ2 0x04
#define bmGPINIRQ3 0x08
#define bmGPINIRQ4 0x10
#define bmGPINIRQ5 0x20
#define bmGPINIRQ6 0x40
#define bmGPINIRQ7 0x80

#define rGPINIEN (23 << 3)
// ---------bits------------
#define bmGPINIEN0 0x01
#define bmGPINIEN1 0x02
#define bmGPINIEN2 0x04
#define bmGPINIEN3 0x08
#define bmGPINIEN4 0x10
#define bmGPINIEN5 0x20
#define bmGPINIEN6 0x40
#define bmGPINIEN7 0x80

#define rGPINPOL (24 << 3)
// ---------bits------------
#define bmGPINPOL0 0x01
#define bmGPINPOL1 0x02
#define bmGPINPOL2 0x04
#define bmGPINPOL3 0x08
#define bmGPINPOL4 0x10
#define bmGPINPOL5 0x20
#define bmGPINPOL6 0x40
#define bmGPINPOL7 0x80
//
#define rMODE (27 << 3)
// ---------bits------------
#define bmHOST 0x01
#define bmLOWSPEED 0x02
#define bmHUBPRE 0x04
#define bmSOFKAENAB 0x08
#define bmSEPIRQ 0x10
#define bmDELAYISO 0x20
#define bmDMPULLDN 0x40
#define bmDPPULLDN 0x80

//
// MAX3421E Registers in HOST mode.
//
#define rRCVFIFO (1 << 3)
#define rSNDFIFO (2 << 3)
// rSUDFIFO defined above
#define rRCVBC (6 << 3)
#define rSNDBC (7 << 3)
// rUSBIRQ defined above
// rUSBIEN defined above
// rUSBCTL defined above
// rCPUCTL defined above
// rPINCTL defined above
// rRevision defined above
// rIOPINS1 defined above
// rIOPINS2 defined above
// rGPINIRQ defined above
// rGPINIEN defined above
// rGPINPOL defined above
#define rHIRQ (25 << 3)
// ---------bits------------
#define bmBUSEVENTIRQ 0x01  // indicates BUS Reset Done or BUS Resume
#define bmRWUIRQ 0x02
#define bmRCVDAVIRQ 0x04
#define bmSNDBAVIRQ 0x08
#define bmSUSDNIRQ 0x10
#define bmCONDETIRQ 0x20
#define bmFRAMEIRQ 0x40
#define bmHXFRDNIRQ 0x80

#define rHIEN (26 << 3)
// ---------bits------------
#define bmBUSEVENTIE 0x01
#define bmRWUIE 0x02
#define bmRCVDAVIE 0x04
#define bmSNDBAVIE 0x08
#define bmSUSDNIE 0x10
#define bmCONDETIE 0x20
#define bmFRAMEIE 0x40
#define bmHXFRDNIE 0x80

// rMODE defined above
#define rPERADDR (28 << 3)
#define rHCTL (29 << 3)
// ---------bits------------
#define bmBUSRST 0x01
#define bmFRMRST 0x02
#define bmSAMPLEBUS 0x04
#define bmSIGRSM 0x08
#define bmRCVTOG0 0x10
#define bmRCVTOG1 0x20
#define bmSNDTOG0 0x40
#define bmSNDTOG1 0x80

#define rHXFR (30 << 3)
// Host XFR token values for writing the HXFR register (R30).
// OR this bit field with the endpoint number in bits 3:0
#define tokSETUP 0x10   // HS=0, ISO=0, OUTNIN=0, SETUP=1
#define tokIN 0x00      // HS=0, ISO=0, OUTNIN=0, SETUP=0
#define tokOUT 0x20     // HS=0, ISO=0, OUTNIN=1, SETUP=0
#define tokINHS 0x80    // HS=1, ISO=0, OUTNIN=0, SETUP=0
#define tokOUTHS 0xA0   // HS=1, ISO=0, OUTNIN=1, SETUP=0
#define tokISOIN 0x40   // HS=0, ISO=1, OUTNIN=0, SETUP=0
#define tokISOOUT 0x60  // HS=0, ISO=1, OUTNIN=1, SETUP=0

#define rHRSL (31 << 3)
// ---------bits------------
#define bmRCVTOGRD 0x10
#define bmSNDTOGRD 0x20
#define bmKSTATUS 0x40
#define bmJSTATUS 0x80
// Host error result codes, the 4 LSB's in the HRSL register.
#define hrSUCCESS 0x00
#define hrBUSY 0x01
#define hrBADREQ 0x02
#define hrUNDEF 0x03
#define hrNAK 0x04
#define hrSTALL 0x05
#define hrTOGERR 0x06
#define hrWRONGPID 0x07
#define hrBADBC 0x08
#define hrPIDERR 0x09
#define hrPKTERR 0x0A
#define hrCRCERR 0x0B
#define hrKERR 0x0C
#define hrJERR 0x0D
#define hrTIMEOUT 0x0E
#define hrBABBLE 0x0F

// Additional error codes locally generated
#define hrSHORTPACKET 0x10
#define hrINVALID 0x11
