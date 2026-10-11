#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#if defined(RULOS_ARM_stm32g4)
#include "stm32g4xx_hal.h"
#else
#include "stm32h5xx_hal.h"
#endif

// The production HAL interface takes 32-bit addresses. Non-PIE test binaries place these static
// objects below 4 GB, so the real backend's address arithmetic runs unchanged on a 64-bit host.
uint8_t _nvconfig_base[2 * TEST_SLOT_BYTES] __attribute__((aligned(TEST_SLOT_BYTES)));
uint32_t test_flash_flags;
uint32_t test_primask;

static bool fail_unlock;
static bool fail_lock;
static bool fail_erase;
static unsigned fail_program_call;
static unsigned unlock_calls;
static unsigned lock_calls;
static unsigned erase_calls;
static unsigned program_calls;
static bool flash_unlocked;

#include "chip/arm/stm32/periph/nvconfig/nvconfig.c"

HAL_StatusTypeDef HAL_FLASH_Unlock(void) {
  unlock_calls++;
  if (fail_unlock) {
    return HAL_ERROR;
  }
  flash_unlocked = true;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_FLASH_Lock(void) {
  lock_calls++;
  if (fail_lock) {
    return HAL_ERROR;
  }
  flash_unlocked = false;
  return HAL_OK;
}

#if defined(RULOS_ARM_stm32g4)
test_flash_regs_t test_flash;

// Map the erase request back to the slot it names, checking the bank/page arithmetic for the
// current DBANK setting. In dual-bank mode pages are numbered from their own bank's base (bank 2
// at flash base + 0x40000); in single-bank mode the 4 KB page index is flash-global.
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase, uint32_t *page_error) {
  assert(flash_unlocked);
  assert(erase->TypeErase == FLASH_TYPEERASE_PAGES);
  uint8_t *slot;
  if (test_flash.OPTR & FLASH_OPTR_DBANK) {
    bool bank2 = TEST_G4_REGION_OFFSET >= 0x40000u;
    assert(erase->Banks == (bank2 ? FLASH_BANK_2 : FLASH_BANK_1));
    assert(erase->NbPages == TEST_SLOT_BYTES / FLASH_PAGE_SIZE);
    uint32_t first = (TEST_G4_REGION_OFFSET - (bank2 ? 0x40000u : 0u)) / FLASH_PAGE_SIZE;
    assert(erase->Page >= first && erase->Page < first + 2 * erase->NbPages);
    assert((erase->Page - first) % erase->NbPages == 0);
    slot = _nvconfig_base + (erase->Page - first) * FLASH_PAGE_SIZE;
  } else {
    assert(erase->Banks == FLASH_BANK_1);
    assert(erase->NbPages == 1);
    uint32_t first = TEST_G4_REGION_OFFSET / FLASH_PAGE_SIZE_128_BITS;
    assert(erase->Page >= first && erase->Page < first + 2);
    slot = _nvconfig_base + (erase->Page - first) * TEST_SLOT_BYTES;
  }
  erase_calls++;
  // A failed operation may already have damaged part of its target slot.
  memset(slot, 0xff, fail_erase ? TEST_SLOT_BYTES / 2 : TEST_SLOT_BYTES);
  *page_error = fail_erase ? erase->Page : UINT32_MAX;
  return fail_erase ? HAL_ERROR : HAL_OK;
}

HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uint32_t dst, uint64_t data) {
  assert(flash_unlocked);
  assert(type == FLASH_TYPEPROGRAM_DOUBLEWORD);
  assert(dst % 8 == 0);
  assert(dst >= (uintptr_t)_nvconfig_base);
  assert(dst + 8 <= (uintptr_t)_nvconfig_base + sizeof(_nvconfig_base));
  program_calls++;
  bool fail = program_calls == fail_program_call;
  memcpy((void *)(uintptr_t)dst, &data, fail ? 4 : 8);
  return fail ? HAL_ERROR : HAL_OK;
}

void FLASH_FlushCaches(void) {
}
#else
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase, uint32_t *sector_error) {
  assert(flash_unlocked);
  assert(erase->TypeErase == FLASH_TYPEERASE_SECTORS);
  assert(erase->Banks == FLASH_BANK_2);
  assert(erase->Sector < 2);
  assert(erase->NbSectors == 1);
  erase_calls++;
  uint8_t *slot = _nvconfig_base + erase->Sector * FLASH_SECTOR_SIZE;
  // A failed operation may already have damaged part of its target slot.
  memset(slot, 0xff, fail_erase ? FLASH_SECTOR_SIZE / 2 : FLASH_SECTOR_SIZE);
  *sector_error = fail_erase ? erase->Sector : UINT32_MAX;
  return fail_erase ? HAL_ERROR : HAL_OK;
}

HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uint32_t dst, uint32_t src) {
  assert(flash_unlocked);
  assert(type == FLASH_TYPEPROGRAM_QUADWORD);
  assert(dst % 16 == 0 && src % 16 == 0);
  assert(dst >= (uintptr_t)_nvconfig_base);
  assert(dst + 16 <= (uintptr_t)_nvconfig_base + sizeof(_nvconfig_base));
  program_calls++;
  bool fail = program_calls == fail_program_call;
  memcpy((void *)(uintptr_t)dst, (const void *)(uintptr_t)src, fail ? 8 : 16);
  return fail ? HAL_ERROR : HAL_OK;
}

HAL_StatusTypeDef HAL_ICACHE_Invalidate(void) {
  return HAL_OK;
}
#endif

static void reboot_backend(void) {
  nv_inited = false;
  nv_active = -1;
  nv_active_seq = 0;
  test_primask = 0;
}

static void reset_flash(void) {
  memset(_nvconfig_base, 0xff, sizeof(_nvconfig_base));
#if defined(RULOS_ARM_stm32g4)
#if defined(TEST_G4_SINGLE_BANK)
  test_flash.OPTR = 0;
#else
  test_flash.OPTR = FLASH_OPTR_DBANK;
#endif
#endif
  fail_unlock = fail_lock = fail_erase = false;
  fail_program_call = 0;
  unlock_calls = lock_calls = erase_calls = program_calls = 0;
  flash_unlocked = false;
  reboot_backend();
}
