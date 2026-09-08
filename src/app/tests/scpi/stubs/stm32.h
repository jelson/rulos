#pragma once

// Only clock initialization is replaced; the tests use the production CDC
// driver and ST USB declarations for all endpoint and callback behavior.
typedef struct {
  unsigned Prescaler, Source, ReloadValue, ErrorLimitValue, HSI48CalibrationValue;
} RCC_CRSInitTypeDef;
typedef struct {
  unsigned OscillatorType, HSI48State;
  struct {
    unsigned PLLState;
  } PLL;
} RCC_OscInitTypeDef;

#define RCC_OSCILLATORTYPE_HSI48                  1
#define RCC_HSI48_ON                              1
#define RCC_PLL_NONE                              0
#define RCC_CRS_SYNC_DIV1                         1
#define RCC_CRS_SYNC_SOURCE_USB                   1
#define RCC_CRS_ERRORLIMIT_DEFAULT                1
#define RCC_CRS_HSI48CALIBRATION_DEFAULT          1
#define __HAL_RCC_CRS_RELOADVALUE_CALCULATE(a, b) ((a) / (b))
#define __HAL_RCC_CRS_CLK_ENABLE()                ((void)0)
#define HAL_OK                                    0

static inline int HAL_RCC_OscConfig(RCC_OscInitTypeDef *cfg) {
  return HAL_OK;
}
static inline void HAL_RCCEx_CRSConfig(RCC_CRSInitTypeDef *cfg) {
}
