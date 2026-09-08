#pragma once

#include "../../scpi/stubs/stm32.h"

typedef struct {
  unsigned PeriphClockSelection, UsbClockSelection;
} RCC_PeriphCLKInitTypeDef;

#define RCC_PERIPHCLK_USB      1
#define RCC_USBCLKSOURCE_HSI48 1

static inline int HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef *cfg) {
  return HAL_OK;
}
static inline void HAL_PWREx_EnableVddUSB(void) {
}
