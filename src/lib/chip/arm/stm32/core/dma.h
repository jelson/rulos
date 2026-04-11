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
 * RULOS DMA abstraction layer.
 *
 * Drivers allocate a DMA channel by logical request (e.g. USART1 TX, TIM2
 * CH1 input capture) without caring whether the underlying chip has a
 * DMAMUX or a fixed request-to-channel mapping. On DMAMUX chips (G0, G4)
 * the core layer picks a free channel dynamically and programs the mux;
 * on fixed-map chips (F0, F1, F3) the core layer looks up the one
 * hardware channel that serves the requested peripheral and fails if
 * another driver already owns it.
 *
 * The core layer owns every DMA interrupt handler. Drivers register TC,
 * HT, and error callbacks at alloc time; the core's IRQ dispatcher
 * clears the hardware flags and invokes the appropriate callback in ISR
 * context.
 *
 * Usage:
 *   rulos_dma_config_t cfg = {
 *     .request         = RULOS_DMA_REQ_USART1_TX,
 *     .direction       = RULOS_DMA_DIR_MEM_TO_PERIPH,
 *     .mode            = RULOS_DMA_MODE_NORMAL,
 *     .periph_width    = RULOS_DMA_WIDTH_BYTE,
 *     .mem_width       = RULOS_DMA_WIDTH_BYTE,
 *     .periph_increment = false,
 *     .mem_increment   = true,
 *     .priority        = 1,
 *     .tc_callback     = my_tx_done,
 *     .user_data       = &my_driver,
 *   };
 *   rulos_dma_channel_t *ch = rulos_dma_alloc(&cfg);
 *   rulos_dma_start(ch, &USART1->TDR, buf, len);
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stm32.h"

typedef struct rulos_dma_channel rulos_dma_channel_t;  // opaque

/*
 * Direction, mode, and priority are pass-through wrappers around the
 * vendor LL constants. init_channel can OR them directly into the DMA
 * CCR register with no runtime conversion. Every supported LL header
 * defines LL_DMA_DIRECTION_*, LL_DMA_MODE_*, and LL_DMA_PRIORITY_*
 * with family-native values.
 */
typedef uint32_t rulos_dma_direction_t;
#define RULOS_DMA_DIR_PERIPH_TO_MEM LL_DMA_DIRECTION_PERIPH_TO_MEMORY
#define RULOS_DMA_DIR_MEM_TO_PERIPH LL_DMA_DIRECTION_MEMORY_TO_PERIPH
#define RULOS_DMA_DIR_MEM_TO_MEM    LL_DMA_DIRECTION_MEMORY_TO_MEMORY

typedef uint32_t rulos_dma_mode_t;
#define RULOS_DMA_MODE_NORMAL   LL_DMA_MODE_NORMAL
#define RULOS_DMA_MODE_CIRCULAR LL_DMA_MODE_CIRCULAR

typedef uint32_t rulos_dma_priority_t;
#define RULOS_DMA_PRIORITY_LOW       LL_DMA_PRIORITY_LOW
#define RULOS_DMA_PRIORITY_MEDIUM    LL_DMA_PRIORITY_MEDIUM
#define RULOS_DMA_PRIORITY_HIGH      LL_DMA_PRIORITY_HIGH
#define RULOS_DMA_PRIORITY_VERYHIGH  LL_DMA_PRIORITY_VERYHIGH

/*
 * Width is the one case where pass-through doesn't work: classic DMA
 * uses DIFFERENT bit positions in CCR for peripheral width (PSIZE,
 * bits [9:8]) and memory width (MSIZE, bits [11:10]), so a single
 * LL_DMA_* constant can't serve both periph_width and mem_width
 * fields. We keep an abstract enum and a small conversion helper
 * in dma.c.
 */
typedef enum {
  RULOS_DMA_WIDTH_BYTE,
  RULOS_DMA_WIDTH_HALFWORD,
  RULOS_DMA_WIDTH_WORD,
} rulos_dma_width_t;

/*
 * Family-agnostic request enum. Same values across every family; the
 * core layer translates to vendor channel numbers (fixed map) or DMAMUX
 * request codes internally. Add new entries here as more drivers are
 * migrated to this API.
 */
typedef enum {
  RULOS_DMA_REQ_NONE = 0,
  RULOS_DMA_REQ_USART1_TX,
  RULOS_DMA_REQ_USART1_RX,
  RULOS_DMA_REQ_USART2_TX,
  RULOS_DMA_REQ_USART2_RX,
  RULOS_DMA_REQ_USART3_TX,
  RULOS_DMA_REQ_USART3_RX,
  RULOS_DMA_REQ_USART4_TX,
  RULOS_DMA_REQ_USART4_RX,
  RULOS_DMA_REQ_USART5_TX,
  RULOS_DMA_REQ_USART5_RX,
  RULOS_DMA_REQ_USART6_TX,
  RULOS_DMA_REQ_USART6_RX,
  RULOS_DMA_REQ_I2C1_TX,
  RULOS_DMA_REQ_I2C1_RX,
  RULOS_DMA_REQ_SPI1_TX,
  RULOS_DMA_REQ_SPI1_RX,
  RULOS_DMA_REQ_SPI2_TX,
  RULOS_DMA_REQ_TIM2_CH1,
  RULOS_DMA_REQ_TIM2_CH2,
  RULOS_DMA_REQ_COUNT_  // keep last
} rulos_dma_request_t;

typedef struct {
  rulos_dma_request_t request;
  rulos_dma_direction_t direction;
  rulos_dma_mode_t mode;
  rulos_dma_width_t periph_width;
  rulos_dma_width_t mem_width;
  bool periph_increment;
  bool mem_increment;
  rulos_dma_priority_t priority;  // RULOS_DMA_PRIORITY_*

  /*
   * Callbacks fire in DMA ISR context. Contract:
   *   - No malloc, no blocking, no rulos_dma_alloc/free.
   *   - Safe to call schedule_now() to defer work to the scheduler.
   *   - The core layer has ALREADY cleared the TC/HT/TE hardware flag
   *     before invoking the callback. Do NOT touch those flags.
   *   - Any callback may be NULL if unused.
   * tc_callback fires at end-of-transfer in normal mode, and at the
   * second-half-complete point of a circular buffer (i.e. the wraparound).
   * ht_callback only fires in circular mode, at the half-buffer point.
   */
  void (*tc_callback)(void *user_data);
  void (*ht_callback)(void *user_data);
  void (*error_callback)(void *user_data);
  void *user_data;
} rulos_dma_config_t;

/*
 * Allocate a DMA channel matching the given request.
 *   DMAMUX chips (G0/G4): picks the first free channel from the pool,
 *     programs the DMAMUX, returns a handle.
 *   Fixed-map chips (F0/F1/F3): looks up the one channel that serves
 *     this peripheral request, returns NULL if already allocated.
 *   H5: returns NULL (GPDMA backend not yet implemented).
 * The config is copied into the channel state; the caller may discard
 * its local copy after this returns.
 */
rulos_dma_channel_t *rulos_dma_alloc(const rulos_dma_config_t *config);

/*
 * Re-run channel init with a new config. Must be called while the
 * channel is stopped. The channel number and request stay the same;
 * only direction / increment / width / mode / callbacks may change.
 * Used by the SD card driver to flip mem_increment between read and
 * write on the same SPI channel.
 */
void rulos_dma_reconfigure(rulos_dma_channel_t *ch,
                           const rulos_dma_config_t *new_config);

/*
 * Program source/dest addresses and transfer count, then enable the
 * channel. For peripheral transfers periph_addr is the fixed peripheral
 * register (e.g. &USART1->TDR) and mem_addr/nitems describe the buffer.
 * `nitems` is in units of the configured data width, not bytes.
 */
void rulos_dma_start(rulos_dma_channel_t *ch,
                     volatile void *periph_addr,
                     void *mem_addr,
                     uint32_t nitems);

// Disable the channel. Callbacks stop firing. Channel remains allocated
// and may be restarted with rulos_dma_start.
void rulos_dma_stop(rulos_dma_channel_t *ch);

// Items not yet transferred. For circular mode this decreases from
// the last `nitems` passed to start, wrapping back up when the buffer
// wraps. Callers compute current write position as buflen - remaining.
uint32_t rulos_dma_get_remaining(const rulos_dma_channel_t *ch);

// Return channel to the pool. Disables the channel first if running.
void rulos_dma_free(rulos_dma_channel_t *ch);


/*
 * --------------------------------------------------------------------
 * LEGACY helpers (to be deleted in Phase 7 of the DMA refactor).
 *
 * These were the only contents of the old core/dma.h. uart.c still
 * includes core/dma.h and uses these until its own migration lands.
 * Once uart.c stops calling them, delete this block.
 *
 * Guarded to G0/G4 only because (a) those are the only families where
 * uart.c actually includes this header, and (b) the stm32xxxx_ll_dma.h
 * header that defines LL_DMA_CHANNEL_N and LL_DMA_ClearFlag_TCn is only
 * pulled in transitively on those families (see src/lib/chip/arm/stm32/
 * stm32.h). Parsing these static inlines on e.g. C0 fails because the
 * LL DMA header isn't included there.
 * --------------------------------------------------------------------
 */

#if defined(RULOS_ARM_stm32g0) || defined(RULOS_ARM_stm32g4)

static inline bool LL_DMA_IsActiveFlag_TC(DMA_TypeDef *DMAx,
                                          int channel_index) {
  switch (channel_index) {
    case LL_DMA_CHANNEL_1:
      return LL_DMA_IsActiveFlag_TC1(DMAx);
    case LL_DMA_CHANNEL_2:
      return LL_DMA_IsActiveFlag_TC2(DMAx);
    case LL_DMA_CHANNEL_3:
      return LL_DMA_IsActiveFlag_TC3(DMAx);
    case LL_DMA_CHANNEL_4:
      return LL_DMA_IsActiveFlag_TC4(DMAx);
    case LL_DMA_CHANNEL_5:
      return LL_DMA_IsActiveFlag_TC5(DMAx);
#ifdef LL_DMA_CHANNEL_6
    case LL_DMA_CHANNEL_6:
      return LL_DMA_IsActiveFlag_TC6(DMAx);
#endif
#ifdef LL_DMA_CHANNEL_7
    case LL_DMA_CHANNEL_7:
      return LL_DMA_IsActiveFlag_TC7(DMAx);
#endif
#ifdef LL_DMA_CHANNEL_8
    case LL_DMA_CHANNEL_8:
      return LL_DMA_IsActiveFlag_TC8(DMAx);
#endif
  }

  return false;
}

static inline void LL_DMA_ClearFlag_TC(DMA_TypeDef *DMAx, int channel_index) {
  switch (channel_index) {
    case LL_DMA_CHANNEL_1:
      LL_DMA_ClearFlag_TC1(DMAx);
      break;
    case LL_DMA_CHANNEL_2:
      LL_DMA_ClearFlag_TC2(DMAx);
      break;
    case LL_DMA_CHANNEL_3:
      LL_DMA_ClearFlag_TC3(DMAx);
      break;
    case LL_DMA_CHANNEL_4:
      LL_DMA_ClearFlag_TC4(DMAx);
      break;
    case LL_DMA_CHANNEL_5:
      LL_DMA_ClearFlag_TC5(DMAx);
      break;
#ifdef LL_DMA_CHANNEL_6
    case LL_DMA_CHANNEL_6:
      LL_DMA_ClearFlag_TC6(DMAx);
      break;
#endif
#ifdef LL_DMA_CHANNEL_7
    case LL_DMA_CHANNEL_7:
      LL_DMA_ClearFlag_TC7(DMAx);
      break;
#endif
#ifdef LL_DMA_CHANNEL_8
    case LL_DMA_CHANNEL_8:
      LL_DMA_ClearFlag_TC8(DMAx);
      break;
#endif
  }
}

static inline bool LL_DMA_IsActiveFlag_HT(DMA_TypeDef *DMAx,
                                          int channel_index) {
  switch (channel_index) {
    case LL_DMA_CHANNEL_1:
      return LL_DMA_IsActiveFlag_HT1(DMAx);
    case LL_DMA_CHANNEL_2:
      return LL_DMA_IsActiveFlag_HT2(DMAx);
    case LL_DMA_CHANNEL_3:
      return LL_DMA_IsActiveFlag_HT3(DMAx);
    case LL_DMA_CHANNEL_4:
      return LL_DMA_IsActiveFlag_HT4(DMAx);
    case LL_DMA_CHANNEL_5:
      return LL_DMA_IsActiveFlag_HT5(DMAx);
#ifdef LL_DMA_CHANNEL_6
    case LL_DMA_CHANNEL_6:
      return LL_DMA_IsActiveFlag_HT6(DMAx);
#endif
#ifdef LL_DMA_CHANNEL_7
    case LL_DMA_CHANNEL_7:
      return LL_DMA_IsActiveFlag_HT7(DMAx);
#endif
#ifdef LL_DMA_CHANNEL_8
    case LL_DMA_CHANNEL_8:
      return LL_DMA_IsActiveFlag_HT8(DMAx);
#endif
  }

  return false;
}

static inline void LL_DMA_ClearFlag_HT(DMA_TypeDef *DMAx, int channel_index) {
  switch (channel_index) {
    case LL_DMA_CHANNEL_1:
      LL_DMA_ClearFlag_HT1(DMAx);
      break;
    case LL_DMA_CHANNEL_2:
      LL_DMA_ClearFlag_HT2(DMAx);
      break;
    case LL_DMA_CHANNEL_3:
      LL_DMA_ClearFlag_HT3(DMAx);
      break;
    case LL_DMA_CHANNEL_4:
      LL_DMA_ClearFlag_HT4(DMAx);
      break;
    case LL_DMA_CHANNEL_5:
      LL_DMA_ClearFlag_HT5(DMAx);
      break;
#ifdef LL_DMA_CHANNEL_6
    case LL_DMA_CHANNEL_6:
      LL_DMA_ClearFlag_HT6(DMAx);
      break;
#endif
#ifdef LL_DMA_CHANNEL_7
    case LL_DMA_CHANNEL_7:
      LL_DMA_ClearFlag_HT7(DMAx);
      break;
#endif
#ifdef LL_DMA_CHANNEL_8
    case LL_DMA_CHANNEL_8:
      LL_DMA_ClearFlag_HT8(DMAx);
      break;
#endif
  }
}

#endif  // RULOS_ARM_stm32g0 || RULOS_ARM_stm32g4 (legacy helper guard)
