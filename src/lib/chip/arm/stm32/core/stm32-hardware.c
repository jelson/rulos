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
#include "core/clock_split.h"
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
#ifdef RULOS_HSE_BYPASS
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;  // sets BYPASS + ON bits
#else
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
#endif
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RULOS_PLLM;
  RCC_OscInitStruct.PLL.PLLN = RULOS_PLLN;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  rulos_hse_try_pll(&RCC_OscInitStruct, config_hsi_pll);
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

#elif defined(RULOS_ARM_stm32h5)
/*
 * STM32H5 clock: PLL1 -> 250 MHz SYSCLK, sourced from HSE (preferred)
 * or HSI (fallback).
 *
 * HSE path (RULOS_USE_HSE): HSE_VALUE / RULOS_PLLM gives the VCI ref;
 * VCI * RULOS_PLLN / RULOS_PLLP gives SYSCLK. For a 10 MHz HSE feeding
 * 250 MHz: PLLM=1, PLLN=50, PLLP=2 (10/1*50/2 = 250). PLLFRACN=0 keeps
 * the math integer. PLLRGE_3 (8-16 MHz VCI) and VCORANGE_WIDE
 * (192-836 MHz VCO) accommodate the resulting 10 MHz / 500 MHz pair.
 *
 * HSI fallback / non-HSE path: HSI=64 MHz divided to 16 MHz VCI then
 * multiplied to 500 MHz VCO via PLLN=31 + PLLFRACN=2048
 * (16 * 31.25 = 500). Copied from ST's NUCLEO-H503RB PWR_STOP example
 * (the only HSI->250 MHz reference in STM32CubeH5).
 *
 * VOS0 is required for SYSCLK > 200 MHz. FLASH_LATENCY_5 is correct
 * for 250 MHz at VOS0 per RM0481. FLASH_PROGRAMMING_DELAY_2 sets
 * WRHIGHFREQ to the 168+ MHz band.
 */
static void config_hsi_pll_h5(RCC_OscInitTypeDef *osc) {
  osc->OscillatorType = RCC_OSCILLATORTYPE_HSI;
  osc->HSIState = RCC_HSI_ON;
  osc->HSIDiv = RCC_HSI_DIV1;
  osc->HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc->PLL.PLLState = RCC_PLL_ON;
  osc->PLL.PLLSource = RCC_PLL1_SOURCE_HSI;
  osc->PLL.PLLM = 4;
  osc->PLL.PLLN = 31;
  osc->PLL.PLLP = 2;
  osc->PLL.PLLQ = 2;
  osc->PLL.PLLR = 2;
  osc->PLL.PLLRGE = RCC_PLL1_VCIRANGE_3;
  osc->PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  osc->PLL.PLLFRACN = 2048;
}

static void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /* Bump voltage scaling to VOS0 and wait for the regulator to settle
   * before attempting the PLL ramp. */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
  }

#ifdef RULOS_USE_HSE
  /* Try HSE + PLL first */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
#ifdef RULOS_HSE_BYPASS
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;  // sets BYPASS + ON bits
#else
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
#endif
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_HSE;
  /* On H5, PLLM/PLLN/PLLP are raw integers (vs G4's RCC_PLLM_DIVn
   * macros). Caller passes the integers via -DRULOS_PLLM=N etc. */
  RCC_OscInitStruct.PLL.PLLM = RULOS_PLLM;
  RCC_OscInitStruct.PLL.PLLN = RULOS_PLLN;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  rulos_hse_try_pll(&RCC_OscInitStruct, config_hsi_pll_h5);
#else
  config_hsi_pll_h5(&RCC_OscInitStruct);
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    __builtin_trap();
  }
#endif

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
    __builtin_trap();
  }

  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

// The H5's flash OTP and read-only data regions live at 0x08FFF000-0x08FFFFFF
// (UID, package code, flash size, calibration values). These addresses fall
// inside the ICACHE-cacheable flash window, but the flash controller doesn't
// service cache-line fills from them -- reads bus-fault. The fix is to mark
// the 4 KiB region as non-cacheable via the MPU so accesses bypass the cache
// and go directly to the flash controller.
//
// See: ST community post "STM32H563 hard fault when trying to read UID"
// (community.st.com tid=584571) and the FLASH_EDATA / ADC calibration MPU
// examples in CubeH5.
static void mpu_make_h5_otp_ro_noncacheable(void) {
  // Attribute slot 0: device-nGnRnE / non-cacheable. The MAIR encoding
  // for "device memory" is 0x00, which is what the Cube examples use for
  // this exact problem.
  MPU_Attributes_InitTypeDef attr = {
      .Number = MPU_ATTRIBUTES_NUMBER0,
      .Attributes = 0,
  };
  HAL_MPU_ConfigMemoryAttributes(&attr);

  MPU_Region_InitTypeDef region = {
      .Enable = MPU_REGION_ENABLE,
      .Number = MPU_REGION_NUMBER0,
      .AttributesIndex = MPU_ATTRIBUTES_NUMBER0,
      .BaseAddress = 0x08FFF000UL,
      .LimitAddress = 0x08FFFFFFUL,
      .AccessPermission = MPU_REGION_ALL_RW,
      .DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE,
      .IsShareable = MPU_ACCESS_NOT_SHAREABLE,
  };

  HAL_MPU_Disable();
  HAL_MPU_ConfigRegion(&region);
  // PRIVILEGED_DEFAULT: privileged code keeps default-memory-map access
  // for any address not covered by an explicit region. RULOS runs at
  // privileged level, so this is what we want -- only the configured
  // region's attributes change, everything else behaves as before.
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
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

#if defined(RULOS_ARM_stm32h5)
  // Must run before ICACHE_Enable so that the cache never tries to fill a
  // line from the OTP/RO region -- which would bus-fault and lock up the
  // chip the first time anyone read e.g. the UID for a USB serial number.
  mpu_make_h5_otp_ro_noncacheable();
#endif

#ifdef HAL_ICACHE_MODULE_ENABLED
  // Enable the instruction cache on chips that have one (STM32H5). ART on
  // G4 is always-on and doesn't need a HAL call.
  HAL_ICACHE_Enable();
#endif

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
#ifdef __HAL_RCC_GPIOH_CLK_ENABLE
  __HAL_RCC_GPIOH_CLK_ENABLE();
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
#ifdef __HAL_RCC_GPDMA1_CLK_ENABLE
  __HAL_RCC_GPDMA1_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPDMA2_CLK_ENABLE
  __HAL_RCC_GPDMA2_CLK_ENABLE();
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

// Split factor (see core/clock_split.h) chosen at init so the hot
// path's multiply stays within u32 without 64-bit math.
static uint32_t g_systick_k_num;
static uint32_t g_systick_load_scaled;

void arm_hal_start_clock_us(uint32_t ticks_per_interrupt,
                            uint32_t us_per_period, clock_handler_t handler,
                            void* data) {
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

  // Precompute the split factor for hal_elapsed_us_in_tick.
  // elapsed_ticks is bounded by SysTick->LOAD (24-bit, up to ~16M).
  clock_split_t sf = clock_split(us_per_period, SysTick->LOAD);
  g_systick_k_num = sf.k_num;
  g_systick_load_scaled = SysTick->LOAD / sf.k_denom;
}

uint32_t hal_elapsed_us_in_tick() {
  // The systick timer counts down.
  //
  // Equivalent (in exact math) to
  //   us_per_period × elapsed_ticks / SysTick->LOAD
  // but with us_per_period pre-factored into k_num × k_denom at init
  // time so k_num × elapsed_ticks stays within u32. All precision loss
  // is absorbed by pre-dividing LOAD by k_denom (see clock_split.h).
  //
  // We avoid 64-bit math here deliberately: on CortexM0+ the u64 form
  // takes about 14μs vs 8μs for this split (measured at 64MHz).
  uint32_t elapsed_ticks = SysTick->LOAD - SysTick->VAL;
  return (g_systick_k_num * elapsed_ticks) / g_systick_load_scaled;
}
