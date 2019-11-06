#pragma once

#include "core/media.h"

#define UM_PREAMBLE0 (-100)
#define UM_PREAMBLE1 (-85)

typedef struct {
  char p0;
  char p1;
  Addr dest_addr;
  uint8_t len;
} UartPreamble;
