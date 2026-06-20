#pragma once

#include <stdint.h>

uint8_t net_compute_checksum(const unsigned char *buf, int size);
