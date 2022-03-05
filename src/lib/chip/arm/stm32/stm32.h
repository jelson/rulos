#pragma once

#if defined(RULOS_ARM_stm32f0)
#include "stm32f0xx.h"
#include "stm32f0xx_ll_gpio.h"
#elif defined(RULOS_ARM_stm32f1)
#include "stm32f1xx.h"
#include "stm32f1xx_ll_gpio.h"
#elif defined(RULOS_ARM_stm32f3)
#include "stm32f3xx.h"
#include "stm32f3xx_ll_gpio.h"
#elif defined(RULOS_ARM_stm32g0)
#include "stm32g0xx.h"
#include "stm32g0xx_hal_dma.h"
#include "stm32g0xx_ll_gpio.h"
#include "stm32g0xx_ll_usart.h"
#elif defined(RULOS_ARM_stm32g4)
#include "stm32g4xx.h"
#include "stm32g4xx_hal_dma.h"
#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_usart.h"

#else
#error "Add support for your STM32 family!"
#include <stophere>
#endif
