#pragma once

#include "core/rulos.h"
#include "periph/uart/uart.h"
#include "periph/uart/linereader.h"

typedef struct {
  float lon, lat;
  UartState_t uart;
  LineReader_t linereader;
  ActivationFuncPtr data_ready_cb_func;
  void* data_ready_cb_data;
} GPSInput;

void gpsinput_init(GPSInput* gpsi, uint8_t uart_id,
                   ActivationFuncPtr data_ready_cb_func,
                   void* data_ready_cb_data);
