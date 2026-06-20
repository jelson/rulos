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

/*
 * STM32H5 nvconfig backend.
 *
 * Power-loss safe via A/B ping-pong: the reserved region is >= 2 flash
 * sectors; each save erase+programs the *inactive* sector while the
 * active one keeps the last good block, and a monotonic sequence
 * number picks the newest valid block at boot. An interrupted
 * erase/program only ever damages the inactive slot.
 *
 * STM32H5 flash is ECC-protected per quad-word: reading a quad-word
 * that is unwritten, or whose program/sector-erase was interrupted,
 * raises an uncorrectable (double) ECC error that escalates to NMI --
 * so merely scanning the slots at boot could fault before USB is up.
 * Handling follows ST's own OEMiROT secure-boot reference
 * (STM32CubeH5 .../NUCLEO-H533RE/.../low_level_flash.c, same chip
 * family): there is no HAL/LL API for an ECC-tolerant read, so this
 * module claims the ECC NMI via the rulos_nmi_claimed hook (core/hse.c
 * owns NMI_Handler) -- clearing the ECCD flag and bumping a counter (the
 * faulting read completes, so the NMI returns and execution resumes);
 * the read path zeroes the counter, copies, and rejects the slot if it
 * fired. Returning false for any non-ECC NMI leaves it to the handler's
 * CSS/HSE failure path.
 *
 * Flash program/erase primitives are family-specific (G4 doubleword,
 * F1 halfword, ...), so like the rulos_dma split this file is H5-only;
 * add a sibling backend when another family needs nvconfig.
 */

#include "periph/nvconfig/nvconfig.h"

#if !defined(RULOS_ARM_stm32h5)
#error "nvconfig currently has only an STM32H5 flash backend"
#endif

#include <string.h>

#include "core/crc32.h"
#include "core/hse.h"
#include "stm32h5xx_hal.h"

// Top-of-flash region the linker reserved (flash_reserve_k); ping-pong
// slots are sector-sized and sector-aligned within it.
extern uint8_t _nvconfig_base[];
extern uint8_t _nvconfig_size[];

// Bumped by rulos_nmi_claimed on each flash double-ECC detection. The
// read path zeroes it, reads, then checks it (ST OEMiROT pattern).
static volatile uint32_t double_ecc_count;

// Claim flash double-ECC NMIs via core/hse.c's first-refusal hook. The
// error sets ECCD and raises NMI but the faulting read still completes,
// so clearing ECCD and counting lets the NMI return and the scan
// resume. Returning false leaves any non-ECC NMI to NMI_Handler's
// CSS/HSE failure path.
bool rulos_nmi_claimed(void) {
  if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_ECCD)) {
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ECCD);
    double_ecc_count++;
    return true;
  }
  return false;
}

#define NVCONFIG_MAGIC 0x434F4E52u  // 'R''N''O''C' (RulOS NV blOC)

typedef struct {
  uint32_t magic;
  uint32_t seq;  // monotonic; highest valid block wins at load
  uint16_t version;
  uint16_t length;  // payload byte count; rejects a stale layout
} nvconfig_hdr_t;

// Framed block = header + payload + CRC32, padded up to the 16 B flash
// program unit. CRC covers header + payload.
#define NV_RAW(payload) (sizeof(nvconfig_hdr_t) + (payload) + sizeof(uint32_t))
#define NV_IMG(payload) ((NV_RAW(payload) + 15u) & ~15u)
#define NV_BUF_LEN      NV_IMG(NVCONFIG_MAX_PAYLOAD)

#define NV_SLOT_BYTES FLASH_SECTOR_SIZE  // one erasable sector per slot
#define NV_NSLOTS     2

static uint8_t *slot_addr(unsigned slot) {
  return _nvconfig_base + (size_t)slot * NV_SLOT_BYTES;
}

// Read one slot into `buf` (>= NV_RAW(len)). Returns true and sets
// *seq only if the slot read with no double-ECC error and holds a
// block with the right magic/version/length and a good CRC.
static bool slot_read(unsigned slot, void *buf, size_t len, uint16_t version, uint32_t *seq) {
  size_t raw = NV_RAW(len);
  bool ok = false;
  uint32_t s = 0;

  double_ecc_count = 0;
  __DSB();
  memcpy(buf, slot_addr(slot), raw);  // torn/blank quad-word -> NMI
  __DSB();
  __ISB();

  const nvconfig_hdr_t *h = (const nvconfig_hdr_t *)buf;
  uint32_t stored;
  memcpy(&stored, (const uint8_t *)buf + sizeof(*h) + len, sizeof(stored));
  if (double_ecc_count == 0u && h->magic == NVCONFIG_MAGIC && h->version == version &&
      h->length == len && stored == crc32(buf, sizeof(*h) + len)) {
    s = h->seq;
    ok = true;
  }

  if (ok) {
    *seq = s;
  }
  return ok;
}

// Cached slot state. The ECC-risky scan of flash happens exactly once
// (the boot read); thereafter the active slot and its sequence live in
// RAM and `nvconfig_save` derives the target slot deterministically.
// It never re-reads a slot before erasing -- doing so (provoking ECC
// NMIs interleaved with erase/program) is what ST's reference flash
// code avoids and what corrupted writes once the flash had been torn.
static bool nv_inited;
static int nv_active = -1;  // slot holding the current good block
static uint32_t nv_active_seq;

static size_t nv_region(void) {
  return (size_t)(uintptr_t)_nvconfig_size;
}

// One-time scan of both slots, ECC-tolerant. Picks the newest valid
// slot into nv_active/nv_active_seq and, if `out`, copies its payload.
static void nv_scan_once(void *out, size_t len, uint16_t version) {
  static uint8_t buf[NV_BUF_LEN];

  // ICACHE caches flash; force the validating reads to hit the array
  // so a torn quad-word actually triggers ECC detection.
  HAL_ICACHE_Invalidate();

  nv_active = -1;
  for (unsigned s = 0; s < NV_NSLOTS; s++) {
    uint32_t seq;
    if (slot_read(s, buf, len, version, &seq) && (nv_active < 0 || seq > nv_active_seq)) {
      nv_active = (int)s;
      nv_active_seq = seq;
      if (out != NULL) {
        memcpy(out, buf + sizeof(nvconfig_hdr_t), len);
      }
    }
  }
  // The scan deliberately read torn/erased slots; drop any sticky ECC
  // detect so it can't shadow a later genuine flash error.
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ECCD);
  nv_inited = true;
}

bool nvconfig_load(void *out, size_t len, uint16_t version) {
  if (len > NVCONFIG_MAX_PAYLOAD || NV_RAW(len) > NV_SLOT_BYTES ||
      nv_region() < NV_NSLOTS * NV_SLOT_BYTES) {
    return false;
  }
  nv_scan_once(out, len, version);
  return nv_active >= 0;
}

void nvconfig_save(const void *data, size_t len, uint16_t version) {
  if (len > NVCONFIG_MAX_PAYLOAD || NV_IMG(len) > NV_SLOT_BYTES ||
      nv_region() < NV_NSLOTS * NV_SLOT_BYTES) {
    return;
  }
  // The active slot/seq come from RAM (the boot scan), never a fresh
  // pre-erase flash read. Lazily scan once only if save runs before
  // any load (every normal flow loads at boot first).
  if (!nv_inited) {
    nv_scan_once(NULL, len, version);
  }

  // Write the *inactive* slot so an interrupted erase/program can only
  // damage the spare; the active slot keeps the last good block.
  unsigned target = (nv_active == 0) ? 1u : 0u;
  uint32_t new_seq = (nv_active < 0) ? 1u : nv_active_seq + 1u;

  static uint8_t __attribute__((aligned(16))) img[NV_BUF_LEN];
  memset(img, 0, sizeof(img));
  nvconfig_hdr_t hdr = {
      .magic = NVCONFIG_MAGIC,
      .seq = new_seq,
      .version = version,
      .length = (uint16_t)len,
  };
  memcpy(img, &hdr, sizeof(hdr));
  memcpy(img + sizeof(hdr), data, len);
  uint32_t crc = crc32(img, sizeof(hdr) + len);
  memcpy(img + sizeof(hdr) + len, &crc, sizeof(crc));

  // STM32H523/H533 flash is DUAL-BANK (per ST: 256 KB or 512 KB dual
  // bank). HAL_FLASHEx_Erase wants the bank plus the sector index
  // *within that bank*, not a flash-global index -- erase ignores the
  // address, so a wrong bank/sector silently no-ops (HAL still returns
  // OK) while program, which uses the absolute address, appears to
  // work. FLASH_BANK_SIZE is the per-bank span (FLASH_SIZE >> 1).
  uint32_t off = (uint32_t)(uintptr_t)slot_addr(target) - FLASH_BASE;
  uint32_t bank = (off < FLASH_BANK_SIZE) ? FLASH_BANK_1 : FLASH_BANK_2;
  uint32_t sector = (off % FLASH_BANK_SIZE) / FLASH_SECTOR_SIZE;

  HAL_FLASH_Unlock();
  FLASH_EraseInitTypeDef erase = {
      .TypeErase = FLASH_TYPEERASE_SECTORS,
      .Banks = bank,
      .Sector = sector,
      .NbSectors = 1,
  };
  bool ok = false;
  uint32_t sector_error;
  if (HAL_FLASHEx_Erase(&erase, &sector_error) == HAL_OK) {
    ok = true;
    uint32_t img_len = (uint32_t)NV_IMG(len);
    for (uint32_t i = 0; i < img_len; i += 16) {
      uint32_t dst = (uint32_t)(uintptr_t)slot_addr(target) + i;
      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, dst, (uint32_t)(uintptr_t)&img[i]) !=
          HAL_OK) {
        ok = false;
        break;
      }
    }
  }
  HAL_FLASH_Lock();

  // Promote the new slot only on a fully successful write. On failure
  // the cached active slot is unchanged, so it still points at the
  // last good block (which was never touched).
  if (ok) {
    nv_active = (int)target;
    nv_active_seq = new_seq;
  }

  // Drop ICACHE lines for the rewritten sector so a later read sees
  // the new block, not a stale line.
  HAL_ICACHE_Invalidate();
}
