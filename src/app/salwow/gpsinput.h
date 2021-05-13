#pragma once

#include "core/rulos.h"
#include "periph/uart/uart.h"

typedef struct {
  float lon, lat;
  ActivationFuncPtr data_ready_cb_func;
  void* data_ready_cb_data;
  char sentence[80];
  char terminator;
  uint8_t recvp;
} GPSInput;

void gpsinput_init(GPSInput* gpsi, uint8_t uart_id,
                   ActivationFuncPtr data_ready_cb_func,
                   void* data_ready_cb_data);
