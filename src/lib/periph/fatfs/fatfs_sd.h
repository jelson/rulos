/*-----------------------------------------------------------------------/
/  Low level disk interface modlue include file   (C)ChaN, 2013          /
/-----------------------------------------------------------------------*/

// RULOS NOTE:
// This is 2 layers of software I (jelson@) have imported from 3rd parties--
//
// 1) FatFS, http://elm-chan.org/fsw/ff/00index_e.html, which
// implements FAT only, and an interface for disk operations (diskio.h).
//
// 2) The SD card implementation of the FatFS disk io interface for STM32, from
// https://github.com/MaJerle/stm32f429/tree/master/00-STM32F429_LIBRARIES/fatfs/drivers
// with some local modifications by jelson@.

#ifndef _DISKIO_DEFINED_SD
#define _DISKIO_DEFINED_SD

#define _USE_WRITE	1	/* 1: Enable disk_write function */
#define _USE_IOCTL	1	/* 1: Enable disk_ioctl fucntion */

#include "ff.h"
#include "diskio.h"
//#include "integer.h"

#include "stm32f3xx.h"
#include "stm32f3xx_ll_rcc.h"
//#include "misc.h"
//#include "defines.h"

//#include "tm_stm32f4_spi.h"
//#include "tm_stm32f4_delay.h"
//#include "tm_stm32f4_gpio.h"
//#include "tm_stm32f4_fatfs.h"

#ifndef FATFS_USE_DETECT_PIN
#define FATFS_USE_DETECT_PIN				0
#endif

#ifndef FATFS_USE_WRITEPROTECT_PIN
#define FATFS_USE_WRITEPROTECT_PIN			0
#endif

#if FATFS_USE_DETECT_PIN > 0
#ifndef FATFS_USE_DETECT_PIN_PIN			
#define FATFS_USE_DETECT_PIN_PORT			GPIOB
#define FATFS_USE_DETECT_PIN_PIN			GPIO_PIN_6
#endif
#endif

#if FATFS_USE_WRITEPROTECT_PIN > 0
#ifndef FATFS_USE_WRITEPROTECT_PIN_PIN
#define FATFS_USE_WRITEPROTECT_PIN_RCC		RCC_AHB1Periph_GPIOB			
#define FATFS_USE_WRITEPROTECT_PIN_PORT		GPIOB
#define FATFS_USE_WRITEPROTECT_PIN_PIN		GPIO_Pin_7
#endif
#endif

#define FATFS_CS_LOW						FATFS_CS_PORT->BSRRH = FATFS_CS_PIN
#define FATFS_CS_HIGH						FATFS_CS_PORT->BSRRL = FATFS_CS_PIN

#endif

