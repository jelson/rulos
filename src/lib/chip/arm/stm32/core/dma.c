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
 * RULOS DMA abstraction layer — stub for families without a real backend.
 *
 * Real backends live in separate files:
 *   dma_ccr.c   — STM32 C0/F0/F1/F3/G0/G4 (classic CCR-register DMA)
 *   dma_gpdma.c — STM32 H5 (GPDMA with linked-list descriptors)
 *
 * This file provides trap-on-call stubs so that any chip family not
 * yet covered by a real backend still links. rulos_dma_alloc returns
 * NULL so callers that check (`if (ch == NULL)` or `assert(ch)`) fail
 * loudly at init time. The other entry points __builtin_trap() because
 * reaching them means a caller passed a bogus handle.
 *
 * See dma.h for the public API.
 */

#include "dma.h"

// Only compile the stubs on families that don't have a real backend.
// If a real backend is active (dma_ccr.c or dma_gpdma.c), it provides
// the same symbols and the linker picks those instead of these stubs.
// The guard prevents duplicate-symbol errors.
#if !defined(RULOS_ARM_stm32c0) && !defined(RULOS_ARM_stm32f0) && \
    !defined(RULOS_ARM_stm32f1) && !defined(RULOS_ARM_stm32f3) && \
    !defined(RULOS_ARM_stm32g0) && !defined(RULOS_ARM_stm32g4) && \
    !defined(RULOS_ARM_stm32h5)

rulos_dma_channel_t *rulos_dma_alloc(const rulos_dma_config_t *config) {
  (void)config;
  return NULL;
}

void rulos_dma_reconfigure(rulos_dma_channel_t *ch,
                           const rulos_dma_config_t *new_config) {
  (void)ch;
  (void)new_config;
  __builtin_trap();
}

void rulos_dma_start(rulos_dma_channel_t *ch, volatile void *periph_addr,
                     void *mem_addr, uint32_t nitems) {
  (void)ch;
  (void)periph_addr;
  (void)mem_addr;
  (void)nitems;
  __builtin_trap();
}

void rulos_dma_stop(rulos_dma_channel_t *ch) {
  (void)ch;
  __builtin_trap();
}

uint32_t rulos_dma_get_remaining(const rulos_dma_channel_t *ch) {
  (void)ch;
  __builtin_trap();
  return 0;
}

void rulos_dma_free(rulos_dma_channel_t *ch) {
  (void)ch;
  __builtin_trap();
}

#endif  // stub guard
