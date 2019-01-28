/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#include "core/net_compute_checksum.h"

uint8_t net_compute_checksum(const unsigned char *buf, int size) {
  int i;
  uint8_t cksum = 0x67;
  for (i = 0; i < size; i++) {
    cksum = cksum * 131 + buf[i];
  }
  return cksum;
}
