#include "core/net_compute_checksum.h"

uint8_t net_compute_checksum(char *buf, int size) {
  int i;
  uint8_t cksum = 0x67;
  for (i = 0; i < size; i++) {
    cksum = cksum * 131 + buf[i];
  }
  return cksum;
}
