#pragma once

#include <stdint.h>

// G4 flash model: 2 KB pages in dual-bank mode, 4 KB in single-bank mode, 8-byte program unit.
#define TEST_SLOT_BYTES          4096u
#define FLASH_PAGE_SIZE          0x800u
#define FLASH_PAGE_SIZE_128_BITS 0x1000u
// Where the two-slot region sits relative to the flash base: in bank 2 (whose base is always
// 0x40000 past the flash base in dual-bank mode) unless the test places it at the top of a
// 64 KB bank 1.
#ifdef TEST_G4_BANK1
#define TEST_G4_REGION_OFFSET (0x10000u - 2u * TEST_SLOT_BYTES)
#else
#define TEST_G4_REGION_OFFSET 0x40000u
#endif
#define FLASH_BANK_SIZE              0x10000u
#define FLASH_BASE                   ((uint32_t)(uintptr_t)_nvconfig_base - TEST_G4_REGION_OFFSET)
#define FLASH_BANK_1                 1u
#define FLASH_BANK_2                 2u
#define FLASH_TYPEERASE_PAGES        3u
#define FLASH_TYPEPROGRAM_DOUBLEWORD 4u
#define FLASH_FLAG_ECCD              1u
#define FLASH_OPTR_DBANK             (1u << 22)

typedef struct {
  uint32_t OPTR;
} test_flash_regs_t;
extern test_flash_regs_t test_flash;
#define FLASH              (&test_flash)
#define READ_BIT(reg, bit) ((reg) & (bit))

typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;

typedef struct {
  uint32_t TypeErase;
  uint32_t Banks;
  uint32_t Page;
  uint32_t NbPages;
} FLASH_EraseInitTypeDef;

extern uint32_t test_flash_flags;
#define __HAL_FLASH_GET_FLAG(flag)   (test_flash_flags & (flag))
#define __HAL_FLASH_CLEAR_FLAG(flag) (test_flash_flags &= ~(flag))
#define __DSB()                      ((void)0)
#define __ISB()                      ((void)0)

HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase, uint32_t *page_error);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uint32_t dst, uint64_t data);
void FLASH_FlushCaches(void);
