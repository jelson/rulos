#pragma once

#include <stdint.h>

#define UNIX_EPOCH_NTP_TIMESTAMP 2208988800

typedef struct {
  // byte 0
  uint8_t mode : 3;           // client is mode 3
  uint8_t version : 3;        // Protocol version
  uint8_t leap_indictor : 2;  // Leap second indicator

  // byte 1
  uint8_t stratum;  // Stratum level of the local clock

  // byte 2
  uint8_t poll;  // Maximum interval between successive messages
  // byte 3
  uint8_t precision;  // Precision of the local clock

  // The following two fields use the "short time format": 16 bits of
  // seconds, 16 bits of fractions-of-a-second

  // Total round trip delay time.
  // byte 4
  uint16_t rootDelay_sec;
  uint16_t rootDelay_frac;

  // Max error in the time measurement
  // byte 8
  uint16_t rootDispersion_sec;
  uint16_t rootDispersion_frac;

  // byte 12
  uint32_t refId;  // Reference clock identifier

  // byte 16
  uint32_t refTime_sec;   // Reference timestamp, seconds
  uint32_t refTime_frac;  // Reference timestamp, fractions of a second

  // byte 24
  uint32_t origTime_sec;   // Originate timestamp, seconds
  uint32_t origTime_frac;  // Originate timestamp, fractions of a second

  // byte 32
  uint32_t rxTime_sec;   // Received timestamp, seconds
  uint32_t rxTime_frac;  // Received timestamp, fractions of a second

  //  byte 40
  uint32_t txTime_sec;   // Transmit timestamp, seconds
  uint32_t txTime_frac;  // Transmit timestamp, fraction of a second
} ntp_packet_t;
