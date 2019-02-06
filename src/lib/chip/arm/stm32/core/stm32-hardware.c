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

#include "core/arm-hal.h"
#include "core/hal.h"
#include "core/hardware.h"

void _init() {}

/**
 * @brief  System Clock Configuration
 *         The system Clock is configured as follow :
 *            System Clock source            = PLL (HSI)
 *            SYSCLK(Hz)                     = 64000000
 *            HCLK(Hz)                       = 64000000
 *            AHB Prescaler                  = 1
 *            APB1 Prescaler                 = 2
 *            APB2 Prescaler                 = 1
 *            PLLMUL                         = 16
 *            Flash Latency(WS)              = 2
 * @param  None
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_ClkInitTypeDef clkinitstruct = {0};
  RCC_OscInitTypeDef oscinitstruct = {0};

  /* Configure PLL ------------------------------------------------------*/
  /* PLL configuration: PLLCLK = (HSI / 2) * PLLMUL = (8 / 2) * 16 = 64 MHz */
  /* PREDIV1 configuration: PREDIV1CLK = PLLCLK / HSEPredivValue = 64 / 1 = 64
   * MHz */
  /* Enable HSI and activate PLL with HSi_DIV2 as source */
  oscinitstruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  oscinitstruct.HSEState = RCC_HSE_OFF;
  oscinitstruct.LSEState = RCC_LSE_OFF;
  oscinitstruct.HSIState = RCC_HSI_ON;
  oscinitstruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  oscinitstruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  oscinitstruct.PLL.PLLState = RCC_PLL_ON;
  oscinitstruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  oscinitstruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&oscinitstruct) != HAL_OK) {
    /* Initialization Error */
    __builtin_trap();
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                             RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2) != HAL_OK) {
    /* Initialization Error */
    __builtin_trap();
  }
}

void hal_init() {
  // Initialize the STM32 HAL library. (Good thing the name didn't
  // collide...)
  HAL_Init();

  SystemClock_Config();
  SystemCoreClockUpdate();
  
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
}

uint32_t arm_hal_get_clock_rate() { return HAL_RCC_GetSysClockFreq(); }

static void* g_timer_data = NULL;
static Handler g_timer_handler = NULL;

void SysTick_Handler() {
  if (g_timer_handler != NULL) {
    g_timer_handler(g_timer_data);
  }
}

void arm_hal_start_clock_us(uint32_t ticks_per_interrupt, Handler handler,
                            void* data) {
  g_timer_handler = handler;
  g_timer_data = data;

  // Setup the timer to fire interrupts at the requested interval.
  SysTick_Config(ticks_per_interrupt);
}
