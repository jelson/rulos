/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "core/arm-hal.h"
#include "core/hal.h"
#include "core/hardware.h"

#if defined(RULOS_ARM_stm32c0)
/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = HSI
  *            SYSCLK(Hz)                     = 48000000
  *            HCLK(Hz)                       = 48000000
  *            AHB Pre-scaler                 = 1
  *            APB1 Pre-scaler                = 1
  *            VDD(V)                         = 3.3
  *            Flash Latency(WS)              = 1
  * @param  None
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /* Activate HSI as clock system source  */
  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSEState            = RCC_HSE_OFF;
  RCC_OscInitStruct.LSEState            = RCC_LSE_OFF;
  RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv              = RCC_HSI_DIV1;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    /* Initialization Error */
    __builtin_trap();
  }

  /* Initializes the SYS, AHB and APB busses clocks  */
  RCC_ClkInitStruct.ClockType      = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1);
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    /* Initialization Error */
    __builtin_trap();
  }
}

#elif defined(RULOS_ARM_stm32f0)
/**
 * @brief  System Clock Configuration
 *         The system Clock is configured as follow :
 *            System Clock source            = PLL (HSI/2)
 *            SYSCLK(Hz)                     = 48000000
 *            HCLK(Hz)                       = 48000000
 *            AHB Prescaler                  = 1
 *            APB1 Prescaler                 = 1
 *            HSI Frequency(Hz)              = 8000000
 *            PREDIV                         = 1
 *            PLLMUL                         = 12
 *            Flash Latency(WS)              = 1
 * @param  None
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* No HSE Oscillator on Nucleo, Activate PLL with HSI/2 as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_OFF;
  RCC_OscInitStruct.LSEState = RCC_LSE_OFF;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    __builtin_trap();
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 clocks
   * dividers */
  RCC_ClkInitStruct.ClockType =
      (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
    __builtin_trap();
  }
}

#elif defined(RULOS_ARM_stm32f1)
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
#elif defined(RULOS_ARM_stm32f3)
/**
 * @brief  Switch the PLL source from HSE  to HSI, and select the PLL as SYSCLK
 *         source.
 *         The system Clock is configured as follow :
 *            System Clock source            = PLL (HSI)
 *            SYSCLK(Hz)                     = 64000000
 *            HCLK(Hz)                       = 64000000
 *            AHB Prescaler                  = 1
 *            APB1 Prescaler                 = 2
 *            APB2 Prescaler                 = 1
 *            HSI Frequency(Hz)              = 8000000
 *            PLLMUL                         = 16
 *            Flash Latency(WS)              = 2
 * @param  None
 * @retval None
 */
static void SystemClock_Config(void) {
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};

  /* HSI Oscillator already ON after system reset, activate PLL with HSI as
   * source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_OFF;
  RCC_OscInitStruct.LSEState = RCC_LSE_OFF;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  // Note, there's an implicit DIV2 in the HSI PLL source.
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    /* Initialization Error */
    __builtin_trap();
  }

  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                 RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    /* Initialization Error */
    __builtin_trap();
  }
}
#elif defined(RULOS_ARM_stm32g0)
static void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the CPU, AHB and APB busses clocks */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
#ifdef RULOS_HSI_SYSCLOCK
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RULOS_HSI_SYSCLOCK;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_OFF;
#else
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
#ifdef RULOS_PLLM
  RCC_OscInitStruct.PLL.PLLM = RULOS_PLLM;
#else
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
#endif

  RCC_OscInitStruct.PLL.PLLN = 8;  // multiplier
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;

#if defined(RCC_PLLQ_SUPPORT)
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
#endif

#ifdef RULOS_PLLR
  RCC_OscInitStruct.PLL.PLLR = RULOS_PLLR;
#else
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
#endif

#endif  // RULOS_HSI_SYSCLOCK

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    __builtin_trap();
  }
  /** Initializes the CPU, AHB and APB busses clocks */
  RCC_ClkInitStruct.ClockType =
      RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
#ifdef RULOS_HSI_SYSCLOCK
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
#else
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
#endif
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    __builtin_trap();
  }
}
#elif defined(RULOS_ARM_stm32g4)
/**
 * @brief  System Clock Configuration
 *         The system Clock is configured as follow :
 *            System Clock source            = PLL (HSI)
 *            SYSCLK(Hz)                     = 170000000
 *            HCLK(Hz)                       = 170000000
 *            AHB Prescaler                  = 1
 *            APB1 Prescaler                 = 1
 *            APB2 Prescaler                 = 1
 *            HSI Frequency(Hz)              = 16000000
 *            PLL_M                          = 4
 *            PLL_N                          = 85
 *            PLL_P                          = 2
 *            PLL_Q                          = 2
 *            PLL_R                          = 2
 *            Flash Latency(WS)              = 8
 * @param  None
 * @retval None
 */

// Default PLL parameters for HSI: 16MHz / 4 * 85 / 2 = 170MHz
#define HSI_PLLM RCC_PLLM_DIV4
#define HSI_PLLN 85

#ifndef RULOS_PLLM
#define RULOS_PLLM HSI_PLLM
#endif
#ifndef RULOS_PLLN
#define RULOS_PLLN HSI_PLLN
#endif

#ifdef RULOS_USE_HSE
// True if HSE (external oscillator) failed at boot or was lost during
// operation. Applications can check this after rulos_hal_init() to detect
// oscillator failure.
bool g_rulos_hse_failed = false;
#endif

static void config_hsi_pll(RCC_OscInitTypeDef *osc) {
  osc->OscillatorType = RCC_OSCILLATORTYPE_HSI;
  osc->HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc->HSIState = RCC_HSI_ON;
  osc->PLL.PLLState = RCC_PLL_ON;
  osc->PLL.PLLSource = RCC_PLLSOURCE_HSI;
  osc->PLL.PLLM = HSI_PLLM;
  osc->PLL.PLLN = HSI_PLLN;
  osc->PLL.PLLP = RCC_PLLP_DIV2;
  osc->PLL.PLLQ = RCC_PLLQ_DIV2;
  osc->PLL.PLLR = RCC_PLLR_DIV2;
}

static void SystemClock_Config(void) {
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};

  /* Enable voltage range 1 boost mode for frequency above 150 Mhz */
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

#ifdef RULOS_USE_HSE
  /* Try HSE + PLL first */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RULOS_PLLM;
  RCC_OscInitStruct.PLL.PLLN = RULOS_PLLN;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) == HAL_OK) {
    /* HSE started successfully. Enable Clock Security System to detect
     * mid-stream HSE failure. On failure, hardware auto-switches to HSI
     * and fires NMI. */
    HAL_RCC_EnableCSS();
  } else {
    /* HSE failed to start. Fall back to HSI + PLL.
     * HSI(16MHz) / 4 * 85 / 2 = 170MHz -- same speed, less accurate. */
    g_rulos_hse_failed = true;
    config_hsi_pll(&RCC_OscInitStruct);
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
      __builtin_trap();
    }
  }
#else
  config_hsi_pll(&RCC_OscInitStruct);
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    __builtin_trap();
  }
#endif

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                 RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_8) != HAL_OK) {
    __builtin_trap();
  }
}

#ifdef RULOS_USE_HSE
// Default NMI handler for CSS. The startup file defines NMI_Handler as a weak
// alias to Default_Handler. This weak definition overrides it but can itself be
// overridden by applications that need custom NMI behavior.
__attribute__((weak))
void NMI_Handler(void) {
  HAL_RCC_NMI_IRQHandler();
}

// CSS callback invoked by HAL_RCC_NMI_IRQHandler when HSE failure is detected.
// Overrides the HAL's weak default. Sets the generic failure flag; apps needing
// additional behavior should override NMI_Handler and act after calling
// HAL_RCC_NMI_IRQHandler().
void HAL_RCC_CSSCallback(void) {
  g_rulos_hse_failed = true;
}
#endif
#else
#error "Your chip needs to know how to init its clock!"
#include <stophere>
#endif

void _init() {
  // entry point used by __libc_init_array()
}

void rulos_hal_init() {
  // Copy .ccmram section from flash to CCM SRAM. This must happen before
  // any CCM-resident functions are called. Safe to run before HAL_Init()
  // since CCM SRAM needs no clock enable.
  extern uint32_t _siccmram, _sccmram, _eccmram;
  for (uint32_t *src = &_siccmram, *dst = &_sccmram; dst < &_eccmram;) {
    *dst++ = *src++;
  }

  // Initialize the STM32 HAL library. (Good thing the name didn't
  // collide...)
  HAL_Init();

  SystemClock_Config();
  SystemCoreClockUpdate();

#ifdef __HAL_RCC_GPIOA_CLK_ENABLE
  __HAL_RCC_GPIOA_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPIOB_CLK_ENABLE
  __HAL_RCC_GPIOB_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPIOC_CLK_ENABLE
  __HAL_RCC_GPIOC_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPIOD_CLK_ENABLE
  __HAL_RCC_GPIOD_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPIOE_CLK_ENABLE
  __HAL_RCC_GPIOE_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPIOF_CLK_ENABLE
  __HAL_RCC_GPIOF_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPIOG_CLK_ENABLE
  __HAL_RCC_GPIOG_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_DMA1_CLK_ENABLE
  __HAL_RCC_DMA1_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_DMA2_CLK_ENABLE
  __HAL_RCC_DMA2_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_DMAMUX1_CLK_ENABLE
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_SYSCFG_CLK_ENABLE
  __HAL_RCC_SYSCFG_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_AFIO_CLK_ENABLE
  __HAL_RCC_AFIO_CLK_ENABLE();
#endif
}

uint32_t arm_hal_get_clock_rate() {
  return HAL_RCC_GetSysClockFreq();
}

static void* g_timer_data = NULL;
static clock_handler_t g_timer_handler = NULL;

void SysTick_Handler() {
  HAL_IncTick();
  if (g_timer_handler != NULL) {
    g_timer_handler(g_timer_data);
  }
}

void arm_hal_start_clock_us(uint32_t ticks_per_interrupt,
                            clock_handler_t handler, void* data) {
  g_timer_handler = handler;
  g_timer_data = data;

  // Setup the timer to fire interrupts at the requested interval.
  SysTick_Config(ticks_per_interrupt);

  // Update the HAL's tick frequency so HAL_GetTick() remains correct.
  // HAL_IncTick() adds uwTickFreq (in ms) on each SysTick interrupt.
  // HAL_Init() configures SysTick for 1ms ticks with uwTickFreq=1;
  // if RULOS changes the SysTick period, we must update uwTickFreq to match.
  uint32_t ms_per_tick =
      ticks_per_interrupt / (HAL_RCC_GetSysClockFreq() / 1000);
  if (ms_per_tick < 1) {
    ms_per_tick = 1;
  }
  uwTickFreq = ms_per_tick;
}

uint16_t hal_elapsed_tenthou_intervals() {
  // The systick timer counts down.
  //
  // elapsed_ticks might be up to 2^24, so multiplying it by 10k can cause
  // 32-bit overflow.
  //
  // It would be more straightforward to just use a 64-bit intermediate value
  // here, but I was hoping to keep 64 bit math out of simply reading the
  // clock. On a CortexM0p running at 64mhz, precise_clock_time_us(), which
  // calls this, takes about 14 usec using 64 bit math and 8 usec doing this One
  // Simple Trick.
  uint32_t elapsed_ticks = SysTick->LOAD - SysTick->VAL;
  if (elapsed_ticks <= 400000) {
    return (10000 * elapsed_ticks) / SysTick->LOAD;
  } else {
    return (100 * elapsed_ticks) / (SysTick->LOAD / 100);
  }
}
