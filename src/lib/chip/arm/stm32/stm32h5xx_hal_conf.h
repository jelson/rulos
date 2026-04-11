/**
  ******************************************************************************
  * @file    stm32h5xx_hal_conf.h
  * @brief   RULOS HAL configuration for STM32H5. Derived from ST's
  *          stm32h5xx_hal_conf_template.h with only the minimal set of HAL
  *          modules needed by RULOS enabled.
  ******************************************************************************
  */

#ifndef STM32H5xx_HAL_CONF_H
#define STM32H5xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ########################## Module Selection ############################## */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_ICACHE_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED

/* Explicitly disabled modules (do not enable without reviewing):
 *   HAL_GTZC_MODULE_ENABLED    -- TrustZone controller
 *   HAL_DCACHE_MODULE_ENABLED  -- DCACHE for external memory interfaces
 *   HAL_RAMCFG_MODULE_ENABLED  -- RAM ECC config (SRAM1 boots usable without)
 * Everything else is left undefined and can be enabled as RULOS peripherals
 * are ported to H5 in follow-up PRs.
 */

/* ############## Oscillator Values (match CMSIS device header) ############## */
#if !defined (HSE_VALUE)
#define HSE_VALUE              25000000UL
#endif
#if !defined (HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT    100UL
#endif
#if !defined (CSI_VALUE)
#define CSI_VALUE              4000000UL
#endif
#if !defined (HSI_VALUE)
#define HSI_VALUE              64000000UL
#endif
#if !defined (HSI48_VALUE)
#define HSI48_VALUE            48000000UL
#endif
#if !defined (LSI_VALUE)
#define LSI_VALUE              32000UL
#endif
#if !defined (LSI_STARTUP_TIME)
#define LSI_STARTUP_TIME       130UL
#endif
#if !defined (LSE_VALUE)
#define LSE_VALUE              32768UL
#endif
#if !defined (LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT    5000UL
#endif
#if !defined (EXTERNAL_CLOCK_VALUE)
#define EXTERNAL_CLOCK_VALUE   12288000UL
#endif

/* ########################### System Configuration ########################## */
#define VDD_VALUE              3300UL
#define TICK_INT_PRIORITY      ((1UL<<__NVIC_PRIO_BITS) - 1UL)
#define USE_RTOS               0U
#define PREFETCH_ENABLE        0U

/* ########################### Register Callbacks ############################ */
#define USE_HAL_ADC_REGISTER_CALLBACKS        0U
#define USE_HAL_CEC_REGISTER_CALLBACKS        0U
#define USE_HAL_CCB_REGISTER_CALLBACKS        0U
#define USE_HAL_COMP_REGISTER_CALLBACKS       0U
#define USE_HAL_CORDIC_REGISTER_CALLBACKS     0U
#define USE_HAL_CRYP_REGISTER_CALLBACKS       0U
#define USE_HAL_DMA2D_REGISTER_CALLBACKS      0U
#define USE_HAL_DAC_REGISTER_CALLBACKS        0U
#define USE_HAL_DCMI_REGISTER_CALLBACKS       0U
#define USE_HAL_DTS_REGISTER_CALLBACKS        0U
#define USE_HAL_ETH_REGISTER_CALLBACKS        0U
#define USE_HAL_FDCAN_REGISTER_CALLBACKS      0U
#define USE_HAL_FMAC_REGISTER_CALLBACKS       0U
#define USE_HAL_GFXTIM_REGISTER_CALLBACKS     0U
#define USE_HAL_NOR_REGISTER_CALLBACKS        0U
#define USE_HAL_HASH_REGISTER_CALLBACKS       0U
#define USE_HAL_HCD_REGISTER_CALLBACKS        0U
#define USE_HAL_I2C_REGISTER_CALLBACKS        0U
#define USE_HAL_I2S_REGISTER_CALLBACKS        0U
#define USE_HAL_I3C_REGISTER_CALLBACKS        0U
#define USE_HAL_IRDA_REGISTER_CALLBACKS       0U
#define USE_HAL_IWDG_REGISTER_CALLBACKS       0U
#define USE_HAL_JPEG_REGISTER_CALLBACKS       0U
#define USE_HAL_LPTIM_REGISTER_CALLBACKS      0U
#define USE_HAL_LTDC_REGISTER_CALLBACKS       0U
#define USE_HAL_MDF_REGISTER_CALLBACKS        0U
#define USE_HAL_MMC_REGISTER_CALLBACKS        0U
#define USE_HAL_NAND_REGISTER_CALLBACKS       0U
#define USE_HAL_OPAMP_REGISTER_CALLBACKS      0U
#define USE_HAL_OTFDEC_REGISTER_CALLBACKS     0U
#define USE_HAL_PCD_REGISTER_CALLBACKS        0U
#define USE_HAL_PKA_REGISTER_CALLBACKS        0U
#define USE_HAL_PLAY_REGISTER_CALLBACKS       0U
#define USE_HAL_RAMCFG_REGISTER_CALLBACKS     0U
#define USE_HAL_RNG_REGISTER_CALLBACKS        0U
#define USE_HAL_RTC_REGISTER_CALLBACKS        0U
#define USE_HAL_SAI_REGISTER_CALLBACKS        0U
#define USE_HAL_SD_REGISTER_CALLBACKS         0U
#define USE_HAL_SDIO_REGISTER_CALLBACKS       0U
#define USE_HAL_SDRAM_REGISTER_CALLBACKS      0U
#define USE_HAL_SMARTCARD_REGISTER_CALLBACKS  0U
#define USE_HAL_SMBUS_REGISTER_CALLBACKS      0U
#define USE_HAL_SPI_REGISTER_CALLBACKS        0U
#define USE_HAL_SRAM_REGISTER_CALLBACKS       0U
#define USE_HAL_TIM_REGISTER_CALLBACKS        0U
#define USE_HAL_UART_REGISTER_CALLBACKS       0U
#define USE_HAL_USART_REGISTER_CALLBACKS      0U
#define USE_HAL_WWDG_REGISTER_CALLBACKS       0U
#define USE_HAL_XSPI_REGISTER_CALLBACKS       0U

/* SPI CRC feature (referenced by stm32h5xx_hal_spi.h even when SPI is off) */
#define USE_SPI_CRC                  1U

/* DMA2D command list (referenced by stm32h5xx_hal_dma2d.h) */
#define USE_DMA2D_COMMAND_LIST_MODE  0U

/* ############################### Includes ################################# */
#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32h5xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32h5xx_hal_gpio.h"
#endif
#ifdef HAL_ICACHE_MODULE_ENABLED
#include "stm32h5xx_hal_icache.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32h5xx_hal_dma.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32h5xx_hal_cortex.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32h5xx_hal_flash.h"
#endif
#ifdef HAL_PCD_MODULE_ENABLED
#include "stm32h5xx_hal_pcd.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32h5xx_hal_pwr.h"
#endif
#ifdef HAL_EXTI_MODULE_ENABLED
#include "stm32h5xx_hal_exti.h"
#endif

/* assert_param macro (full-assert not used in RULOS) */
#define assert_param(expr) ((void)0U)

#ifdef __cplusplus
}
#endif

#endif /* STM32H5xx_HAL_CONF_H */
