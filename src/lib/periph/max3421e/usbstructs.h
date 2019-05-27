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

// These are generic USB structures. If we ever have a second way to
// talk to USB other than the max3421 in RULOS, this should be
// factored out to a usb peripheral directory, perhaps along with some
// other utility functions, rather than living in the max3421e directory.

typedef struct {
  uint8_t low;
  uint8_t high;
} __attribute__((packed)) usb_word_t;

/* USB Setup Packet Structure */
typedef struct {
  union {
    uint8_t bmRequestType;  // Bitmap of request type

    struct {
      uint8_t recipient : 5;  // Recipient of the request
      uint8_t type : 2;       // Type of request
      uint8_t direction : 1;  // Direction of data transfer
    } __attribute__((packed));
  } ReqType_u;
  uint8_t bRequest;  // Request

  // Arguments whose meaning depends on bRequest
  usb_word_t wValue;
  usb_word_t wIndex;
  usb_word_t wLength;
} __attribute__((packed)) USB_SETUP_PACKET;

/* USB string language structure */
typedef struct {
  // Length of this descriptor.
  uint8_t bLength;

  // Descriptor type; equal to 0x3
  uint8_t bDescriptorType;

  // Pointer to an array of languages; you have to put this struct at
  // the start of a larger buffer.
  usb_word_t LANGID[0];
} __attribute((packed)) USB_LANG_DESCRIPTOR;

/* Device descriptor structure */
typedef struct {
  // Length of this descriptor.
  uint8_t bLength;

  // DEVICE descriptor type (USB_DESCRIPTOR_DEVICE).
  uint8_t bDescriptorType;

  // USB Spec Release Number (BCD).
  uint16_t bcdUSB;

  // Class code (assigned by the USB-IF). 0xFF-Vendor specific.
  uint8_t bDeviceClass;

  // Subclass code (assigned by the USB-IF).
  uint8_t bDeviceSubClass;

  // Protocol code (assigned by the USB-IF). 0xFF-Vendor specific.
  uint8_t bDeviceProtocol;

  // Maximum packet size for endpoint 0.
  uint8_t bMaxPacketSize0;

  // Vendor ID (assigned by the USB-IF).
  uint16_t idVendor;

  // Product ID (assigned by the vendor).
  uint16_t idProduct;

  // Device release number (BCD).
  uint16_t bcdDevice;

  // Index of String Descriptor describing the manufacturer.
  uint8_t iManufacturer;

  // Index of String Descriptor describing the product.
  uint8_t iProduct;

  // Index of String Descriptor with the device's serial number.
  uint8_t iSerialNumber;

  // Number of possible configurations.
  uint8_t bNumConfigurations;
} __attribute__((packed)) USB_DEVICE_DESCRIPTOR;

/* Configuration descriptor structure */
typedef struct {
  // Length of this descriptor.
  uint8_t bLength;

  // CONFIGURATION descriptor type (USB_DESCRIPTOR_CONFIGURATION).
  uint8_t bDescriptorType;

  // Total length of all descriptors for this configuration.
  uint16_t wTotalLength;

  // Number of interfaces in this configuration.
  uint8_t bNumInterfaces;

  // Value of this configuration (1 based).
  uint8_t bConfigurationValue;

  // Index of String Descriptor describing the configuration.
  uint8_t iConfiguration;

  // Configuration characteristics.
  uint8_t bmAttributes;

  // Maximum power consumed by this configuration.
  uint8_t bMaxPower;
} __attribute__((packed)) USB_CONFIGURATION_DESCRIPTOR;

/* Interface descriptor structure */
typedef struct {
  // Length of this descriptor.
  uint8_t bLength;

  // INTERFACE descriptor type (USB_DESCRIPTOR_INTERFACE).
  uint8_t bDescriptorType;

  // Number of this interface (0 based).
  uint8_t bInterfaceNumber;

  // Value of this alternate interface setting.
  uint8_t bAlternateSetting;

  // Number of endpoints in this interface.
  uint8_t bNumEndpoints;

  // Class code (assigned by the USB-IF).  0xFF-Vendor specific.
  uint8_t bInterfaceClass;

  // Subclass code (assigned by the USB-IF).
  uint8_t bInterfaceSubClass;

  // Protocol code (assigned by the USB-IF).  0xFF-Vendor specific.
  uint8_t bInterfaceProtocol;

  // Index of String Descriptor describing the interface.
  uint8_t iInterface;
} __attribute__((packed)) USB_INTERFACE_DESCRIPTOR;

/* Endpoint descriptor structure */
typedef struct {
  // Length of this descriptor.
  uint8_t bLength;

  // ENDPOINT descriptor type (USB_DESCRIPTOR_ENDPOINT).
  uint8_t bDescriptorType;

  // Endpoint address. Bit 7 indicates direction (0=OUT, 1=IN).
  uint8_t bEndpointAddress;

  // Endpoint transfer type.
  uint8_t bmAttributes;

  // Maximum packet size.
  uint16_t wMaxPacketSize;

  // Polling interval in frames.
  uint8_t bInterval;
} __attribute__((packed)) USB_ENDPOINT_DESCRIPTOR;

/* HID descriptor */
typedef struct {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint16_t bcdHID;  // HID class specification release
  uint8_t bCountryCode;
  uint8_t bNumDescriptors;  // Number of additional class specific descriptors
  uint8_t bDescrType;       // Type of class descriptor
  uint16_t wDescriptorLength;  // Total size of the Report descriptor
} __attribute__((packed)) USB_HID_DESCRIPTOR;

// clang-format off
/* Standard Device Requests */
#define USB_REQUEST_GET_STATUS                  0       // Standard Device Request - GET STATUS
#define USB_REQUEST_CLEAR_FEATURE               1       // Standard Device Request - CLEAR FEATURE
#define USB_REQUEST_SET_FEATURE                 3       // Standard Device Request - SET FEATURE
#define USB_REQUEST_SET_ADDRESS                 5       // Standard Device Request - SET ADDRESS
#define USB_REQUEST_GET_DESCRIPTOR              6       // Standard Device Request - GET DESCRIPTOR
#define USB_REQUEST_SET_DESCRIPTOR              7       // Standard Device Request - SET DESCRIPTOR
#define USB_REQUEST_GET_CONFIGURATION           8       // Standard Device Request - GET CONFIGURATION
#define USB_REQUEST_SET_CONFIGURATION           9       // Standard Device Request - SET CONFIGURATION
#define USB_REQUEST_GET_INTERFACE               10      // Standard Device Request - GET INTERFACE
#define USB_REQUEST_SET_INTERFACE               11      // Standard Device Request - SET INTERFACE
#define USB_REQUEST_SYNCH_FRAME                 12      // Standard Device Request - SYNCH FRAME

#define USB_FEATURE_ENDPOINT_HALT               0       // CLEAR/SET FEATURE - Endpoint Halt
#define USB_FEATURE_DEVICE_REMOTE_WAKEUP        1       // CLEAR/SET FEATURE - Device remote wake-up
#define USB_FEATURE_TEST_MODE                   2       // CLEAR/SET FEATURE - Test mode

/* Setup Data Constants */

#define USB_SETUP_HOST_TO_DEVICE                0x00    // Device Request bmRequestType transfer direction - host to device transfer
#define USB_SETUP_DEVICE_TO_HOST                0x80    // Device Request bmRequestType transfer direction - device to host transfer
#define USB_SETUP_TYPE_STANDARD                 0x00    // Device Request bmRequestType type - standard
#define USB_SETUP_TYPE_CLASS                    0x20    // Device Request bmRequestType type - class
#define USB_SETUP_TYPE_VENDOR                   0x40    // Device Request bmRequestType type - vendor
#define USB_SETUP_RECIPIENT_DEVICE              0x00    // Device Request bmRequestType recipient - device
#define USB_SETUP_RECIPIENT_INTERFACE           0x01    // Device Request bmRequestType recipient - interface
#define USB_SETUP_RECIPIENT_ENDPOINT            0x02    // Device Request bmRequestType recipient - endpoint
#define USB_SETUP_RECIPIENT_OTHER               0x03    // Device Request bmRequestType recipient - other


/* Common setup data constant combinations  */
#define bmREQ_GET_DESCR     USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_STANDARD|USB_SETUP_RECIPIENT_DEVICE     //get descriptor request type
#define bmREQ_SET           USB_SETUP_HOST_TO_DEVICE|USB_SETUP_TYPE_STANDARD|USB_SETUP_RECIPIENT_DEVICE     //set request type for all but 'set feature' and 'set interface'
#define bmREQ_CL_GET_INTF   USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_CLASS|USB_SETUP_RECIPIENT_INTERFACE     //get interface request type


/* USB descriptors  */

#define USB_DESCRIPTOR_DEVICE                   0x01    // bDescriptorType for a Device Descriptor.
#define USB_DESCRIPTOR_CONFIGURATION            0x02    // bDescriptorType for a Configuration Descriptor.
#define USB_DESCRIPTOR_STRING                   0x03    // bDescriptorType for a String Descriptor.
#define USB_DESCRIPTOR_INTERFACE                0x04    // bDescriptorType for an Interface Descriptor.
#define USB_DESCRIPTOR_ENDPOINT                 0x05    // bDescriptorType for an Endpoint Descriptor.
#define USB_DESCRIPTOR_DEVICE_QUALIFIER         0x06    // bDescriptorType for a Device Qualifier.
#define USB_DESCRIPTOR_OTHER_SPEED              0x07    // bDescriptorType for a Other Speed Configuration.
#define USB_DESCRIPTOR_INTERFACE_POWER          0x08    // bDescriptorType for Interface Power.
#define USB_DESCRIPTOR_OTG                      0x09    // bDescriptorType for an OTG Descriptor.

#define HID_DESCRIPTOR_HID                      0x21


/* OTG SET FEATURE Constants    */
#define OTG_FEATURE_B_HNP_ENABLE                3       // SET FEATURE OTG - Enable B device to perform HNP
#define OTG_FEATURE_A_HNP_SUPPORT               4       // SET FEATURE OTG - A device supports HNP
#define OTG_FEATURE_A_ALT_HNP_SUPPORT           5       // SET FEATURE OTG - Another port on the A device supports HNP

/* USB Endpoint Transfer Types  */
#define USB_TRANSFER_TYPE_CONTROL               0x00    // Endpoint is a control endpoint.
#define USB_TRANSFER_TYPE_ISOCHRONOUS           0x01    // Endpoint is an isochronous endpoint.
#define USB_TRANSFER_TYPE_BULK                  0x02    // Endpoint is a bulk endpoint.
#define USB_TRANSFER_TYPE_INTERRUPT             0x03    // Endpoint is an interrupt endpoint.
#define bmUSB_TRANSFER_TYPE                     0x03    // bit mask to separate transfer type from ISO attributes


/* Standard Feature Selectors for CLEAR_FEATURE Requests    */
#define USB_FEATURE_ENDPOINT_STALL              0       // Endpoint recipient
#define USB_FEATURE_DEVICE_REMOTE_WAKEUP        1       // Device recipient
#define USB_FEATURE_TEST_MODE                   2       // Device recipient
// clang-format on
