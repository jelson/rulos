#pragma once

#include <stdint.h>

#ifndef FLASH_SECTOR_SIZE
#define FLASH_SECTOR_SIZE 8192u
#endif
#define FLASH_BANK_SIZE            (2u * FLASH_SECTOR_SIZE)
#define FLASH_BASE                 ((uint32_t)(uintptr_t)_nvconfig_base - FLASH_BANK_SIZE)
#define FLASH_BANK_1               1u
#define FLASH_BANK_2               2u
#define FLASH_TYPEERASE_SECTORS    1u
#define FLASH_TYPEPROGRAM_QUADWORD 2u
#define FLASH_FLAG_ECCD            1u

typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;

typedef struct {
  uint32_t TypeErase;
  uint32_t Banks;
  uint32_t Sector;
  uint32_t NbSectors;
} FLASH_EraseInitTypeDef;

extern uint32_t test_flash_flags;
#define __HAL_FLASH_GET_FLAG(flag)   (test_flash_flags & (flag))
#define __HAL_FLASH_CLEAR_FLAG(flag) (test_flash_flags &= ~(flag))
#define __DSB()                      ((void)0)
#define __ISB()                      ((void)0)

HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase, uint32_t *sector_error);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uint32_t dst, uint32_t src);
HAL_StatusTypeDef HAL_ICACHE_Invalidate(void);
