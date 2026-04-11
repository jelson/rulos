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
 * CCR register with no runtime conversion. The vendor LL headers use
 * different symbol names for classic DMA (F/G families) vs GPDMA (H5)
 * but the value at the rulos_dma API boundary is opaque -- callers
 * just pass the symbolic RULOS_DMA_* names.
 */
typedef uint32_t rulos_dma_direction_t;
#define RULOS_DMA_DIR_PERIPH_TO_MEM LL_DMA_DIRECTION_PERIPH_TO_MEMORY
#define RULOS_DMA_DIR_MEM_TO_PERIPH LL_DMA_DIRECTION_MEMORY_TO_PERIPH
#define RULOS_DMA_DIR_MEM_TO_MEM    LL_DMA_DIRECTION_MEMORY_TO_MEMORY

typedef uint32_t rulos_dma_mode_t;
#if defined(RULOS_ARM_stm32h5)
// GPDMA. Normal exists as LL_DMA_NORMAL (no _MODE_ infix). Circular
// mode on GPDMA requires linked-list descriptors which the H5 backend
// doesn't implement yet -- use a sentinel so init_channel can reject
// RULOS_DMA_MODE_CIRCULAR loudly if a caller asks for it.
#define RULOS_DMA_MODE_NORMAL   LL_DMA_NORMAL
#define RULOS_DMA_MODE_CIRCULAR 0xFFFFFFFFU
#else
#define RULOS_DMA_MODE_NORMAL   LL_DMA_MODE_NORMAL
#define RULOS_DMA_MODE_CIRCULAR LL_DMA_MODE_CIRCULAR
#endif

typedef uint32_t rulos_dma_priority_t;
#if defined(RULOS_ARM_stm32h5)
// GPDMA priority is a two-axis (priority level, weight) scheme rather
// than the classic four-level scheme. Map the four RULOS names to the
// four GPDMA levels that exist.
#define RULOS_DMA_PRIORITY_LOW       LL_DMA_LOW_PRIORITY_LOW_WEIGHT
#define RULOS_DMA_PRIORITY_MEDIUM    LL_DMA_LOW_PRIORITY_MID_WEIGHT
#define RULOS_DMA_PRIORITY_HIGH      LL_DMA_LOW_PRIORITY_HIGH_WEIGHT
#define RULOS_DMA_PRIORITY_VERYHIGH  LL_DMA_HIGH_PRIORITY
#else
#define RULOS_DMA_PRIORITY_LOW       LL_DMA_PRIORITY_LOW
#define RULOS_DMA_PRIORITY_MEDIUM    LL_DMA_PRIORITY_MEDIUM
#define RULOS_DMA_PRIORITY_HIGH      LL_DMA_PRIORITY_HIGH
#define RULOS_DMA_PRIORITY_VERYHIGH  LL_DMA_PRIORITY_VERYHIGH
#endif

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
