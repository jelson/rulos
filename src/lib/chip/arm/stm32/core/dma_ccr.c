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
 * RULOS DMA backend for STM32 families with the classic CCR-based DMA
 * controller (DMA1/DMA2 with per-channel CCR configuration registers).
 *
 * Supported families:
 *   - STM32C0 (DMAMUX, 3 channels, merged IRQs)
 *   - STM32F0 (fixed-map, 5 channels, merged IRQs)
 *   - STM32F1 (fixed-map, 7 channels, per-channel IRQs)
 *   - STM32F3 (fixed-map, 7+5 channels, per-channel IRQs)
 *   - STM32G0 (DMAMUX, 7-12 channels, merged IRQs)
 *   - STM32G4 (DMAMUX, 6-8+6-8 channels, per-channel IRQs)
 *
 * See dma.h for the public API. This file is compiled for the families
 * listed above; other families get dma_gpdma.c (H5) or the stub in
 * dma.c.
 */

#include "dma.h"

#include <stddef.h>

#include "core/hal.h"
#include "core/hardware.h"  // for CCMRAM
#include "stm32.h"

// ============================================================================
// Classic DMA backend (DMA1/DMA2 with CCR register layout).
//
// Covers every STM32 family RULOS supports except H5:
//   - Fixed-map families (no DMAMUX): F0, F1, F3
//   - DMAMUX families (dynamic peripheral→channel routing): C0, G0, G4
//
// Everything except the allocator, init_channel's DMAMUX step, and
// the per-chip IRQ handlers is shared. The pool, request translation
// (either DMAMUX constant or fixed slot), public API, and IRQ
// dispatcher use LL macros that are defined identically across all
// six families.
// ============================================================================
#if defined(RULOS_ARM_stm32c0) || defined(RULOS_ARM_stm32f0) || \
    defined(RULOS_ARM_stm32f1) || defined(RULOS_ARM_stm32f3) || \
    defined(RULOS_ARM_stm32g0) || defined(RULOS_ARM_stm32g4)

#if defined(RULOS_ARM_stm32c0) || defined(RULOS_ARM_stm32g0) || \
    defined(RULOS_ARM_stm32g4)
#define RULOS_DMA_HAS_DMAMUX 1
#else
#define RULOS_DMA_HAS_DMAMUX 0
#endif

/*
 * Per-channel NVIC IRQ number. Two flavors:
 *
 *   Per-channel IRQs (G4, F1, F3): every DMA channel has its own
 *     dedicated IRQ line and handler.
 *
 *   Merged IRQs (C0, F0, G0): multiple channels share an IRQ line.
 *     Channels 2-3 share one line on C0, F0, and G0. Channels 4+
 *     (plus DMA2 on G0B1) share another "merged" line whose name
 *     depends on the chip variant. C0 only has three DMA channels
 *     so it doesn't need a Ch4+ merged line. Multiple slots holding
 *     the same IRQ number is fine -- NVIC_EnableIRQ is idempotent.
 */
#if defined(RULOS_ARM_stm32g4) || defined(RULOS_ARM_stm32f1) || \
    defined(RULOS_ARM_stm32f3)
// Per-channel IRQ families.
#define RULOS_DMA1_CH1_IRQN  DMA1_Channel1_IRQn
#define RULOS_DMA1_CH2_IRQN  DMA1_Channel2_IRQn
#define RULOS_DMA1_CH3_IRQN  DMA1_Channel3_IRQn
#define RULOS_DMA1_CH4_IRQN  DMA1_Channel4_IRQn
#define RULOS_DMA1_CH5_IRQN  DMA1_Channel5_IRQn
#ifdef DMA1_Channel6_BASE
#define RULOS_DMA1_CH6_IRQN  DMA1_Channel6_IRQn
#endif
#ifdef DMA1_Channel7_BASE
#define RULOS_DMA1_CH7_IRQN  DMA1_Channel7_IRQn
#endif
#ifdef DMA1_Channel8_BASE
#define RULOS_DMA1_CH8_IRQN  DMA1_Channel8_IRQn
#endif
#ifdef DMA2_Channel1_BASE
#define RULOS_DMA2_CH1_IRQN  DMA2_Channel1_IRQn
#define RULOS_DMA2_CH2_IRQN  DMA2_Channel2_IRQn
#define RULOS_DMA2_CH3_IRQN  DMA2_Channel3_IRQn
#if defined(RULOS_ARM_stm32f1)
// F1 high/XL-density merges DMA2 Ch4-5. CMSIS aliases DMA2_Channel4_IRQn
// to DMA2_Channel4_5_IRQn via a macro, so the Ch4 line below already
// points at the merged IRQn; we just have to use that same macro for
// Ch5 since DMA2_Channel5_IRQn doesn't exist on F1.
#define RULOS_DMA2_CH4_IRQN  DMA2_Channel4_IRQn
#define RULOS_DMA2_CH5_IRQN  DMA2_Channel4_IRQn
#else
#define RULOS_DMA2_CH4_IRQN  DMA2_Channel4_IRQn
#define RULOS_DMA2_CH5_IRQN  DMA2_Channel5_IRQn
#endif
#ifdef DMA2_Channel6_BASE
#define RULOS_DMA2_CH6_IRQN  DMA2_Channel6_IRQn
#endif
#ifdef DMA2_Channel7_BASE
#define RULOS_DMA2_CH7_IRQN  DMA2_Channel7_IRQn
#endif
#ifdef DMA2_Channel8_BASE
#define RULOS_DMA2_CH8_IRQN  DMA2_Channel8_IRQn
#endif
#endif  // DMA2_Channel1_BASE
#elif defined(RULOS_ARM_stm32g0)
// G0: merged IRQ lines.
#define RULOS_DMA1_CH1_IRQN  DMA1_Channel1_IRQn
#define RULOS_DMA1_CH2_IRQN  DMA1_Channel2_3_IRQn
#define RULOS_DMA1_CH3_IRQN  DMA1_Channel2_3_IRQn
#if defined(STM32G0B1xx)
#define RULOS_DMA_MERGED_IRQN  DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn
#else
#define RULOS_DMA_MERGED_IRQN  DMA1_Ch4_5_DMAMUX1_OVR_IRQn
#endif
#define RULOS_DMA1_CH4_IRQN  RULOS_DMA_MERGED_IRQN
#define RULOS_DMA1_CH5_IRQN  RULOS_DMA_MERGED_IRQN
#ifdef DMA1_Channel6_BASE
#define RULOS_DMA1_CH6_IRQN  RULOS_DMA_MERGED_IRQN
#endif
#ifdef DMA1_Channel7_BASE
#define RULOS_DMA1_CH7_IRQN  RULOS_DMA_MERGED_IRQN
#endif
#ifdef DMA2_Channel1_BASE
#define RULOS_DMA2_CH1_IRQN  RULOS_DMA_MERGED_IRQN
#define RULOS_DMA2_CH2_IRQN  RULOS_DMA_MERGED_IRQN
#define RULOS_DMA2_CH3_IRQN  RULOS_DMA_MERGED_IRQN
#define RULOS_DMA2_CH4_IRQN  RULOS_DMA_MERGED_IRQN
#define RULOS_DMA2_CH5_IRQN  RULOS_DMA_MERGED_IRQN
#endif
#elif defined(RULOS_ARM_stm32c0)
// C0: Channel1 standalone, Ch2-3 merged. Only 3 channels total.
#define RULOS_DMA1_CH1_IRQN  DMA1_Channel1_IRQn
#define RULOS_DMA1_CH2_IRQN  DMA1_Channel2_3_IRQn
#define RULOS_DMA1_CH3_IRQN  DMA1_Channel2_3_IRQn
#else  // F0: Channel1 standalone, Ch2-3 and Ch4-5 merged.
#define RULOS_DMA1_CH1_IRQN  DMA1_Channel1_IRQn
#define RULOS_DMA1_CH2_IRQN  DMA1_Channel2_3_IRQn
#define RULOS_DMA1_CH3_IRQN  DMA1_Channel2_3_IRQn
#define RULOS_DMA1_CH4_IRQN  DMA1_Channel4_5_IRQn
#define RULOS_DMA1_CH5_IRQN  DMA1_Channel4_5_IRQn
#endif  // per-channel vs merged IRQ naming

/*
 * Per-channel hardware descriptor. Immutable, const, lives in .rodata
 * (flash only, zero RAM cost). The array is sized to cover all 16
 * possible G4 channels (DMA1 channels 1..8, DMA2 channels 1..8).
 * Variants like STM32G431 that only expose 6 channels per DMA leave
 * the upper slots zero-initialized; the allocator skips entries
 * whose `dma` pointer is NULL.
 *
 * The per-channel IRQ handlers at the bottom of this file hardcode
 * the array index they manage, so the designated initializer positions
 * are load-bearing:
 *   [0..7]  = DMA1 channels 1..8
 *   [8..15] = DMA2 channels 1..8
 */
typedef struct {
  DMA_TypeDef *dma;              // DMA1 or DMA2 (NULL = slot not populated)
  DMA_Channel_TypeDef *hw_ch;    // DMA1_Channel1, ...
  uint32_t ll_channel;           // LL_DMA_CHANNEL_1..8 (values 0..7)
  IRQn_Type irqn;
} dma_channel_hw_t;

#define DMA_CHANNEL_SLOTS 16

static const dma_channel_hw_t g_hw[DMA_CHANNEL_SLOTS] = {
#ifdef DMA1_Channel1_BASE
    [0] = {DMA1, DMA1_Channel1, LL_DMA_CHANNEL_1, RULOS_DMA1_CH1_IRQN},
#endif
#ifdef DMA1_Channel2_BASE
    [1] = {DMA1, DMA1_Channel2, LL_DMA_CHANNEL_2, RULOS_DMA1_CH2_IRQN},
#endif
#ifdef DMA1_Channel3_BASE
    [2] = {DMA1, DMA1_Channel3, LL_DMA_CHANNEL_3, RULOS_DMA1_CH3_IRQN},
#endif
#ifdef DMA1_Channel4_BASE
    [3] = {DMA1, DMA1_Channel4, LL_DMA_CHANNEL_4, RULOS_DMA1_CH4_IRQN},
#endif
#ifdef DMA1_Channel5_BASE
    [4] = {DMA1, DMA1_Channel5, LL_DMA_CHANNEL_5, RULOS_DMA1_CH5_IRQN},
#endif
#ifdef DMA1_Channel6_BASE
    [5] = {DMA1, DMA1_Channel6, LL_DMA_CHANNEL_6, RULOS_DMA1_CH6_IRQN},
#endif
#ifdef DMA1_Channel7_BASE
    [6] = {DMA1, DMA1_Channel7, LL_DMA_CHANNEL_7, RULOS_DMA1_CH7_IRQN},
#endif
#ifdef DMA1_Channel8_BASE
    [7] = {DMA1, DMA1_Channel8, LL_DMA_CHANNEL_8, RULOS_DMA1_CH8_IRQN},
#endif
#ifdef DMA2_Channel1_BASE
    [8] = {DMA2, DMA2_Channel1, LL_DMA_CHANNEL_1, RULOS_DMA2_CH1_IRQN},
#endif
#ifdef DMA2_Channel2_BASE
    [9] = {DMA2, DMA2_Channel2, LL_DMA_CHANNEL_2, RULOS_DMA2_CH2_IRQN},
#endif
#ifdef DMA2_Channel3_BASE
    [10] = {DMA2, DMA2_Channel3, LL_DMA_CHANNEL_3, RULOS_DMA2_CH3_IRQN},
#endif
#ifdef DMA2_Channel4_BASE
    [11] = {DMA2, DMA2_Channel4, LL_DMA_CHANNEL_4, RULOS_DMA2_CH4_IRQN},
#endif
#ifdef DMA2_Channel5_BASE
    [12] = {DMA2, DMA2_Channel5, LL_DMA_CHANNEL_5, RULOS_DMA2_CH5_IRQN},
#endif
#ifdef DMA2_Channel6_BASE
    [13] = {DMA2, DMA2_Channel6, LL_DMA_CHANNEL_6, RULOS_DMA2_CH6_IRQN},
#endif
#ifdef DMA2_Channel7_BASE
    [14] = {DMA2, DMA2_Channel7, LL_DMA_CHANNEL_7, RULOS_DMA2_CH7_IRQN},
#endif
#ifdef DMA2_Channel8_BASE
    [15] = {DMA2, DMA2_Channel8, LL_DMA_CHANNEL_8, RULOS_DMA2_CH8_IRQN},
#endif
};

/*
 * Per-channel mutable state. Lives in .bss (zero-initialized). Only
 * the fields we need in the IRQ dispatch hot path are kept; the rest
 * of the rulos_dma_config_t passed to rulos_dma_alloc() is written
 * to DMA hardware registers by init_channel() and then discarded.
 *
 * Opaque API handle: rulos_dma_channel_t* is a cast pointer into this
 * array. The array index (and therefore the matching g_hw entry) is
 * recovered by pointer subtraction.
 */
typedef struct {
  bool allocated;
  void (*tc_callback)(void *user_data);
  void (*ht_callback)(void *user_data);
  void (*error_callback)(void *user_data);
  void *user_data;
} dma_channel_state_t;

static dma_channel_state_t g_state[DMA_CHANNEL_SLOTS];

// Convert an opaque handle back to its slot index.
static inline int state_to_idx(const rulos_dma_channel_t *ch) {
  return (const dma_channel_state_t *)ch - g_state;
}

#if !RULOS_DMA_HAS_DMAMUX
/*
 * Fixed-map families (F0/F1/F3) wire each DMA-capable peripheral to a
 * specific hardware channel at manufacturing time (see the chip RM's
 * DMA request mapping table). There's no dynamic routing -- if two
 * peripherals happen to share a channel, they can't coexist.
 *
 * Values are stored as `slot_index + 1` so that an unset entry
 * (left at zero by designated initializers) unambiguously means
 * "this request has no fixed mapping on this chip". Lookup subtracts
 * one to recover the real slot index.
 *
 * Sourced from the DMA request-to-channel tables in the vendor
 * reference manuals:
 *   F0: RM0360 Table 33 ("DMA1 requests for each channel")
 *   F1: RM0008 Table 78 ("Summary of DMA1 requests for each channel")
 *   F3: RM0316 Table 29 ("Summary of the DMA1 requests for each channel")
 */
static const uint8_t g_fixed_slot_plus_one[RULOS_DMA_REQ_COUNT_] = {
#if defined(RULOS_ARM_stm32f0)
    [RULOS_DMA_REQ_USART1_TX] = 1 + 1,  // DMA1 Ch2 (conflicts with I2C1_TX/SPI1_RX)
    [RULOS_DMA_REQ_USART1_RX] = 2 + 1,  // DMA1 Ch3 (conflicts with I2C1_RX/SPI1_TX)
    [RULOS_DMA_REQ_I2C1_TX]   = 1 + 1,  // DMA1 Ch2 (conflicts with USART1_TX/SPI1_RX)
    [RULOS_DMA_REQ_I2C1_RX]   = 2 + 1,  // DMA1 Ch3 (conflicts with USART1_RX/SPI1_TX)
    [RULOS_DMA_REQ_SPI1_RX]   = 1 + 1,  // DMA1 Ch2 (conflicts with USART1_TX/I2C1_TX)
    [RULOS_DMA_REQ_SPI1_TX]   = 2 + 1,  // DMA1 Ch3 (conflicts with USART1_RX/I2C1_RX)
    [RULOS_DMA_REQ_SPI2_TX]   = 4 + 1,  // DMA1 Ch5
#elif defined(RULOS_ARM_stm32f1) || defined(RULOS_ARM_stm32f3)
    [RULOS_DMA_REQ_SPI1_RX]   = 1 + 1,  // DMA1 Ch2
    [RULOS_DMA_REQ_SPI1_TX]   = 2 + 1,  // DMA1 Ch3
    [RULOS_DMA_REQ_USART1_TX] = 3 + 1,  // DMA1 Ch4
    [RULOS_DMA_REQ_USART1_RX] = 4 + 1,  // DMA1 Ch5 (conflicts with SPI2_TX)
    [RULOS_DMA_REQ_I2C1_TX]   = 5 + 1,  // DMA1 Ch6
    [RULOS_DMA_REQ_I2C1_RX]   = 6 + 1,  // DMA1 Ch7
    [RULOS_DMA_REQ_SPI2_TX]   = 4 + 1,  // DMA1 Ch5 (conflicts with USART1_RX)
#endif
};
#endif  // !RULOS_DMA_HAS_DMAMUX

/*
 * RULOS request enum -> G4 DMAMUX request code. Designated initializers
 * leave unused slots at 0 (= LL_DMAMUX_REQ_MEM2MEM, safe sentinel).
 */
#if RULOS_DMA_HAS_DMAMUX
// Every peripheral request entry is individually guarded because small
// G0 variants (G030/G031) lack USART3+, TIM2 DMA, SPI2 DMA, etc. The
// allocator checks for a zero entry and fails alloc if a driver asks
// for a request the chip doesn't have.
static const uint32_t g_dmamux_req[RULOS_DMA_REQ_COUNT_] = {
#ifdef LL_DMAMUX_REQ_USART1_TX
    [RULOS_DMA_REQ_USART1_TX] = LL_DMAMUX_REQ_USART1_TX,
    [RULOS_DMA_REQ_USART1_RX] = LL_DMAMUX_REQ_USART1_RX,
#endif
#ifdef LL_DMAMUX_REQ_USART2_TX
    [RULOS_DMA_REQ_USART2_TX] = LL_DMAMUX_REQ_USART2_TX,
    [RULOS_DMA_REQ_USART2_RX] = LL_DMAMUX_REQ_USART2_RX,
#endif
#ifdef LL_DMAMUX_REQ_USART3_TX
    [RULOS_DMA_REQ_USART3_TX] = LL_DMAMUX_REQ_USART3_TX,
    [RULOS_DMA_REQ_USART3_RX] = LL_DMAMUX_REQ_USART3_RX,
#endif
// G4 calls these UART4/UART5; G0 calls them USART4..USART6. The two
// naming conventions never coexist on the same chip, so the #ifdef
// guards pick exactly one branch per family.
#ifdef LL_DMAMUX_REQ_UART4_TX
    [RULOS_DMA_REQ_USART4_TX] = LL_DMAMUX_REQ_UART4_TX,
    [RULOS_DMA_REQ_USART4_RX] = LL_DMAMUX_REQ_UART4_RX,
#endif
#ifdef LL_DMAMUX_REQ_UART5_TX
    [RULOS_DMA_REQ_USART5_TX] = LL_DMAMUX_REQ_UART5_TX,
    [RULOS_DMA_REQ_USART5_RX] = LL_DMAMUX_REQ_UART5_RX,
#endif
#ifdef LL_DMAMUX_REQ_USART4_TX
    [RULOS_DMA_REQ_USART4_TX] = LL_DMAMUX_REQ_USART4_TX,
    [RULOS_DMA_REQ_USART4_RX] = LL_DMAMUX_REQ_USART4_RX,
#endif
#ifdef LL_DMAMUX_REQ_USART5_TX
    [RULOS_DMA_REQ_USART5_TX] = LL_DMAMUX_REQ_USART5_TX,
    [RULOS_DMA_REQ_USART5_RX] = LL_DMAMUX_REQ_USART5_RX,
#endif
#ifdef LL_DMAMUX_REQ_USART6_TX
    [RULOS_DMA_REQ_USART6_TX] = LL_DMAMUX_REQ_USART6_TX,
    [RULOS_DMA_REQ_USART6_RX] = LL_DMAMUX_REQ_USART6_RX,
#endif
#ifdef LL_DMAMUX_REQ_I2C1_TX
    [RULOS_DMA_REQ_I2C1_TX] = LL_DMAMUX_REQ_I2C1_TX,
    [RULOS_DMA_REQ_I2C1_RX] = LL_DMAMUX_REQ_I2C1_RX,
#endif
#ifdef LL_DMAMUX_REQ_SPI1_TX
    [RULOS_DMA_REQ_SPI1_TX] = LL_DMAMUX_REQ_SPI1_TX,
    [RULOS_DMA_REQ_SPI1_RX] = LL_DMAMUX_REQ_SPI1_RX,
#endif
#ifdef LL_DMAMUX_REQ_SPI2_TX
    [RULOS_DMA_REQ_SPI2_TX] = LL_DMAMUX_REQ_SPI2_TX,
#endif
#ifdef LL_DMAMUX_REQ_TIM2_CH1
    [RULOS_DMA_REQ_TIM2_CH1] = LL_DMAMUX_REQ_TIM2_CH1,
    [RULOS_DMA_REQ_TIM2_CH2] = LL_DMAMUX_REQ_TIM2_CH2,
#endif
};
#endif  // RULOS_DMA_HAS_DMAMUX

// ----------------------------------------------------------------------------
// Private channel-indexed flag dispatch helpers.
//
// The classic-DMA LL headers (stm32fNxx_ll_dma.h, stm32gNxx_ll_dma.h)
// only expose per-channel flag helpers as
// LL_DMA_IsActiveFlag_TCn(DMAx) / LL_DMA_ClearFlag_TCn(DMAx) where
// the channel number N is part of the function name. Our dispatcher
// needs to index into the flag helpers by a runtime LL_DMA_CHANNEL_*
// value, so we wrap them in switch-based helpers once per file.
// ----------------------------------------------------------------------------

CCMRAM static bool ll_dma_is_active_flag_tc(DMA_TypeDef *dma, uint32_t ch) {
  switch (ch) {
    case LL_DMA_CHANNEL_1:
      return LL_DMA_IsActiveFlag_TC1(dma);
    case LL_DMA_CHANNEL_2:
      return LL_DMA_IsActiveFlag_TC2(dma);
    case LL_DMA_CHANNEL_3:
      return LL_DMA_IsActiveFlag_TC3(dma);
#ifdef LL_DMA_CHANNEL_4
    case LL_DMA_CHANNEL_4:
      return LL_DMA_IsActiveFlag_TC4(dma);
#endif
#ifdef LL_DMA_CHANNEL_5
    case LL_DMA_CHANNEL_5:
      return LL_DMA_IsActiveFlag_TC5(dma);
#endif
#ifdef LL_DMA_CHANNEL_6
    case LL_DMA_CHANNEL_6:
      return LL_DMA_IsActiveFlag_TC6(dma);
#endif
#ifdef LL_DMA_CHANNEL_7
    case LL_DMA_CHANNEL_7:
      return LL_DMA_IsActiveFlag_TC7(dma);
#endif
#ifdef LL_DMA_CHANNEL_8
    case LL_DMA_CHANNEL_8:
      return LL_DMA_IsActiveFlag_TC8(dma);
#endif
  }
  return false;
}

CCMRAM static void ll_dma_clear_flag_tc(DMA_TypeDef *dma, uint32_t ch) {
  switch (ch) {
    case LL_DMA_CHANNEL_1:
      LL_DMA_ClearFlag_TC1(dma);
      break;
    case LL_DMA_CHANNEL_2:
      LL_DMA_ClearFlag_TC2(dma);
      break;
    case LL_DMA_CHANNEL_3:
      LL_DMA_ClearFlag_TC3(dma);
      break;
#ifdef LL_DMA_CHANNEL_4
    case LL_DMA_CHANNEL_4:
      LL_DMA_ClearFlag_TC4(dma);
      break;
#endif
#ifdef LL_DMA_CHANNEL_5
    case LL_DMA_CHANNEL_5:
      LL_DMA_ClearFlag_TC5(dma);
      break;
#endif
#ifdef LL_DMA_CHANNEL_6
    case LL_DMA_CHANNEL_6:
      LL_DMA_ClearFlag_TC6(dma);
      break;
#endif
#ifdef LL_DMA_CHANNEL_7
    case LL_DMA_CHANNEL_7:
      LL_DMA_ClearFlag_TC7(dma);
      break;
#endif
#ifdef LL_DMA_CHANNEL_8
    case LL_DMA_CHANNEL_8:
      LL_DMA_ClearFlag_TC8(dma);
      break;
#endif
  }
}

CCMRAM static bool ll_dma_is_active_flag_ht(DMA_TypeDef *dma, uint32_t ch) {
  switch (ch) {
    case LL_DMA_CHANNEL_1:
      return LL_DMA_IsActiveFlag_HT1(dma);
    case LL_DMA_CHANNEL_2:
      return LL_DMA_IsActiveFlag_HT2(dma);
    case LL_DMA_CHANNEL_3:
      return LL_DMA_IsActiveFlag_HT3(dma);
#ifdef LL_DMA_CHANNEL_4
    case LL_DMA_CHANNEL_4:
      return LL_DMA_IsActiveFlag_HT4(dma);
#endif
#ifdef LL_DMA_CHANNEL_5
    case LL_DMA_CHANNEL_5:
      return LL_DMA_IsActiveFlag_HT5(dma);
#endif
#ifdef LL_DMA_CHANNEL_6
    case LL_DMA_CHANNEL_6:
      return LL_DMA_IsActiveFlag_HT6(dma);
#endif
#ifdef LL_DMA_CHANNEL_7
    case LL_DMA_CHANNEL_7:
      return LL_DMA_IsActiveFlag_HT7(dma);
#endif
#ifdef LL_DMA_CHANNEL_8
    case LL_DMA_CHANNEL_8:
      return LL_DMA_IsActiveFlag_HT8(dma);
#endif
  }
  return false;
}

CCMRAM static void ll_dma_clear_flag_ht(DMA_TypeDef *dma, uint32_t ch) {
  switch (ch) {
    case LL_DMA_CHANNEL_1:
      LL_DMA_ClearFlag_HT1(dma);
      break;
    case LL_DMA_CHANNEL_2:
      LL_DMA_ClearFlag_HT2(dma);
      break;
    case LL_DMA_CHANNEL_3:
      LL_DMA_ClearFlag_HT3(dma);
      break;
#ifdef LL_DMA_CHANNEL_4
    case LL_DMA_CHANNEL_4:
      LL_DMA_ClearFlag_HT4(dma);
      break;
#endif
#ifdef LL_DMA_CHANNEL_5
    case LL_DMA_CHANNEL_5:
      LL_DMA_ClearFlag_HT5(dma);
      break;
#endif
#ifdef LL_DMA_CHANNEL_6
    case LL_DMA_CHANNEL_6:
      LL_DMA_ClearFlag_HT6(dma);
      break;
#endif
#ifdef LL_DMA_CHANNEL_7
    case LL_DMA_CHANNEL_7:
      LL_DMA_ClearFlag_HT7(dma);
      break;
#endif
#ifdef LL_DMA_CHANNEL_8
    case LL_DMA_CHANNEL_8:
      LL_DMA_ClearFlag_HT8(dma);
      break;
#endif
  }
}

CCMRAM static bool ll_dma_is_active_flag_te(DMA_TypeDef *dma, uint32_t ch) {
  switch (ch) {
    case LL_DMA_CHANNEL_1:
      return LL_DMA_IsActiveFlag_TE1(dma);
    case LL_DMA_CHANNEL_2:
      return LL_DMA_IsActiveFlag_TE2(dma);
    case LL_DMA_CHANNEL_3:
      return LL_DMA_IsActiveFlag_TE3(dma);
#ifdef LL_DMA_CHANNEL_4
    case LL_DMA_CHANNEL_4:
      return LL_DMA_IsActiveFlag_TE4(dma);
#endif
#ifdef LL_DMA_CHANNEL_5
    case LL_DMA_CHANNEL_5:
      return LL_DMA_IsActiveFlag_TE5(dma);
#endif
#ifdef LL_DMA_CHANNEL_6
    case LL_DMA_CHANNEL_6:
      return LL_DMA_IsActiveFlag_TE6(dma);
#endif
#ifdef LL_DMA_CHANNEL_7
    case LL_DMA_CHANNEL_7:
      return LL_DMA_IsActiveFlag_TE7(dma);
#endif
#ifdef LL_DMA_CHANNEL_8
    case LL_DMA_CHANNEL_8:
      return LL_DMA_IsActiveFlag_TE8(dma);
#endif
  }
  return false;
}

CCMRAM static void ll_dma_clear_flag_te(DMA_TypeDef *dma, uint32_t ch) {
  switch (ch) {
    case LL_DMA_CHANNEL_1:
      LL_DMA_ClearFlag_TE1(dma);
      break;
    case LL_DMA_CHANNEL_2:
      LL_DMA_ClearFlag_TE2(dma);
      break;
    case LL_DMA_CHANNEL_3:
      LL_DMA_ClearFlag_TE3(dma);
      break;
#ifdef LL_DMA_CHANNEL_4
    case LL_DMA_CHANNEL_4:
      LL_DMA_ClearFlag_TE4(dma);
      break;
#endif
#ifdef LL_DMA_CHANNEL_5
    case LL_DMA_CHANNEL_5:
      LL_DMA_ClearFlag_TE5(dma);
      break;
#endif
#ifdef LL_DMA_CHANNEL_6
    case LL_DMA_CHANNEL_6:
      LL_DMA_ClearFlag_TE6(dma);
      break;
#endif
#ifdef LL_DMA_CHANNEL_7
    case LL_DMA_CHANNEL_7:
      LL_DMA_ClearFlag_TE7(dma);
      break;
#endif
#ifdef LL_DMA_CHANNEL_8
    case LL_DMA_CHANNEL_8:
      LL_DMA_ClearFlag_TE8(dma);
      break;
#endif
  }
}

// ----------------------------------------------------------------------------
// Channel init (called from alloc and reconfigure)
//
// Programs the DMA hardware registers from `config`, and caches the
// small set of fields the ISR dispatcher needs (callbacks + user_data)
// into g_state[idx]. Everything else in `config` is applied to the
// hardware and then dropped on the floor -- no per-channel copy of
// the full rulos_dma_config_t is retained.
// ----------------------------------------------------------------------------

static uint32_t periph_width_to_ll(rulos_dma_width_t w) {
  switch (w) {
    case RULOS_DMA_WIDTH_BYTE:
      return LL_DMA_PDATAALIGN_BYTE;
    case RULOS_DMA_WIDTH_HALFWORD:
      return LL_DMA_PDATAALIGN_HALFWORD;
    case RULOS_DMA_WIDTH_WORD:
      return LL_DMA_PDATAALIGN_WORD;
  }
  return LL_DMA_PDATAALIGN_BYTE;
}

static uint32_t mem_width_to_ll(rulos_dma_width_t w) {
  switch (w) {
    case RULOS_DMA_WIDTH_BYTE:
      return LL_DMA_MDATAALIGN_BYTE;
    case RULOS_DMA_WIDTH_HALFWORD:
      return LL_DMA_MDATAALIGN_HALFWORD;
    case RULOS_DMA_WIDTH_WORD:
      return LL_DMA_MDATAALIGN_WORD;
  }
  return LL_DMA_MDATAALIGN_BYTE;
}

static void init_channel(int idx, const rulos_dma_config_t *c) {
  const dma_channel_hw_t *hw = &g_hw[idx];
  dma_channel_state_t *s = &g_state[idx];

  LL_DMA_DisableChannel(hw->dma, hw->ll_channel);

  // Clear any stale TC/HT/TE flags left over from a previous owner of
  // this slot. If a reconfigure or free+alloc lands on a slot whose
  // IFCR flags are still set from the last transfer, the subsequent
  // LL_DMA_EnableIT_TC below would fire the NVIC immediately and the
  // fresh callback would see a spurious interrupt.
  ll_dma_clear_flag_tc(hw->dma, hw->ll_channel);
  ll_dma_clear_flag_ht(hw->dma, hw->ll_channel);
  ll_dma_clear_flag_te(hw->dma, hw->ll_channel);

  // Build the CCR configuration word and apply it in one shot.
  // Direction, mode, and priority come straight from the caller's
  // config (the RULOS_DMA_* constants for those three are just
  // #defines that expand to the vendor LL constants). Width needs a
  // small conversion because LL_DMA_PDATAALIGN_* and LL_DMA_MDATAALIGN_*
  // land in different CCR bit fields.
  uint32_t cfg =
      c->direction | c->mode |
      (c->periph_increment ? LL_DMA_PERIPH_INCREMENT : LL_DMA_PERIPH_NOINCREMENT) |
      (c->mem_increment ? LL_DMA_MEMORY_INCREMENT : LL_DMA_MEMORY_NOINCREMENT) |
      periph_width_to_ll(c->periph_width) | mem_width_to_ll(c->mem_width) |
      c->priority;
  LL_DMA_ConfigTransfer(hw->dma, hw->ll_channel, cfg);

#if RULOS_DMA_HAS_DMAMUX
  // Route the peripheral request through DMAMUX. Fixed-map families
  // skip this step entirely -- the channel is hardwired to the
  // peripheral at manufacturing time.
  LL_DMA_SetPeriphRequest(hw->dma, hw->ll_channel, g_dmamux_req[c->request]);
#endif

  // Enable the interrupts corresponding to the callbacks we'll dispatch.
  LL_DMA_EnableIT_TC(hw->dma, hw->ll_channel);
  LL_DMA_EnableIT_TE(hw->dma, hw->ll_channel);
  if (c->mode == RULOS_DMA_MODE_CIRCULAR) {
    LL_DMA_EnableIT_HT(hw->dma, hw->ll_channel);
  }

  // Cache only the callbacks + user_data -- the rest of the config is
  // in the hardware now.
  s->tc_callback = c->tc_callback;
  s->ht_callback = c->ht_callback;
  s->error_callback = c->error_callback;
  s->user_data = c->user_data;
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

rulos_dma_channel_t *rulos_dma_alloc(const rulos_dma_config_t *config) {
  if (config == NULL || config->request == RULOS_DMA_REQ_NONE ||
      config->request >= RULOS_DMA_REQ_COUNT_) {
    return NULL;
  }

  int found_idx = -1;
  rulos_irq_state_t irq = hal_start_atomic();

#if RULOS_DMA_HAS_DMAMUX
  if (g_dmamux_req[config->request] == 0) {
    // The request enum value has no DMAMUX mapping on this family.
    hal_end_atomic(irq);
    return NULL;
  }
  // First-fit scan: any free channel can serve any peripheral.
  for (int i = 0; i < DMA_CHANNEL_SLOTS; i++) {
    if (g_hw[i].dma == NULL) continue;  // slot not populated on this variant
    if (g_state[i].allocated) continue;
    g_state[i].allocated = true;
    found_idx = i;
    break;
  }
#else
  // Fixed-map lookup: each peripheral request has exactly one channel
  // it's allowed to use.
  uint8_t slot_plus_one = g_fixed_slot_plus_one[config->request];
  if (slot_plus_one != 0) {
    int slot = slot_plus_one - 1;
    if (g_hw[slot].dma != NULL && !g_state[slot].allocated) {
      g_state[slot].allocated = true;
      found_idx = slot;
    }
  }
#endif

  hal_end_atomic(irq);

  if (found_idx < 0) {
    return NULL;
  }

  init_channel(found_idx, config);

  // Priority 1 is the same priority RULOS already used for its DMA IRQs
  // (see the old uart.c init path). SysTick runs at the lowest priority
  // (15), so DMA ISRs can safely call schedule_now() via the RULOS
  // scheduler's ISR-safe path.
  HAL_NVIC_SetPriority(g_hw[found_idx].irqn, 1, 0);
  HAL_NVIC_EnableIRQ(g_hw[found_idx].irqn);

  return (rulos_dma_channel_t *)&g_state[found_idx];
}

void rulos_dma_reconfigure(rulos_dma_channel_t *ch,
                           const rulos_dma_config_t *new_config) {
  init_channel(state_to_idx(ch), new_config);
}

void rulos_dma_start(rulos_dma_channel_t *ch, volatile void *periph_addr,
                     void *mem_addr, uint32_t nitems) {
  const dma_channel_hw_t *hw = &g_hw[state_to_idx(ch)];

  LL_DMA_DisableChannel(hw->dma, hw->ll_channel);
  LL_DMA_SetDataLength(hw->dma, hw->ll_channel, nitems);

  // Direction was already written to CCR by init_channel; we only pick
  // src/dst address ordering here based on what LL_DMA_ConfigAddresses
  // expects for the current direction.
  const uint32_t ccr_dir =
      LL_DMA_GetDataTransferDirection(hw->dma, hw->ll_channel);
  if (ccr_dir == LL_DMA_DIRECTION_MEMORY_TO_PERIPH) {
    LL_DMA_ConfigAddresses(hw->dma, hw->ll_channel, (uint32_t)mem_addr,
                           (uint32_t)periph_addr, ccr_dir);
  } else {
    LL_DMA_ConfigAddresses(hw->dma, hw->ll_channel, (uint32_t)periph_addr,
                           (uint32_t)mem_addr, ccr_dir);
  }

  LL_DMA_EnableChannel(hw->dma, hw->ll_channel);
}

void rulos_dma_stop(rulos_dma_channel_t *ch) {
  const dma_channel_hw_t *hw = &g_hw[state_to_idx(ch)];
  LL_DMA_DisableChannel(hw->dma, hw->ll_channel);

  // Clear any TC/HT/TE flags left over from the transfer we just
  // halted. Without this, a transfer that completes in hardware
  // right before we disable the channel leaves the IFCR flag set
  // and the NVIC line pending; the dispatcher would then fire a
  // spurious callback as soon as interrupts are next enabled (even
  // if the next action is another rulos_dma_start on the same
  // channel -- start doesn't re-run init_channel, so the flags
  // would survive across the restart). The channel is already
  // disabled above, so no new flag can be set between here and
  // return.
  ll_dma_clear_flag_tc(hw->dma, hw->ll_channel);
  ll_dma_clear_flag_ht(hw->dma, hw->ll_channel);
  ll_dma_clear_flag_te(hw->dma, hw->ll_channel);
}

uint32_t rulos_dma_get_remaining(const rulos_dma_channel_t *ch) {
  const dma_channel_hw_t *hw = &g_hw[state_to_idx(ch)];
  return LL_DMA_GetDataLength(hw->dma, hw->ll_channel);
}

void rulos_dma_free(rulos_dma_channel_t *ch) {
  const int idx = state_to_idx(ch);
  const dma_channel_hw_t *hw = &g_hw[idx];
  LL_DMA_DisableChannel(hw->dma, hw->ll_channel);

  // Silence this channel at the DMA-hardware level by disabling its
  // TC/HT/TE interrupt enables, rather than disabling the NVIC line.
  // On merged-IRQ families (G0/F0) the NVIC line is shared with up
  // to 9 other DMA channels, so HAL_NVIC_DisableIRQ(hw->irqn) would
  // knock out every other allocated channel on the same line.
  LL_DMA_DisableIT_TC(hw->dma, hw->ll_channel);
  LL_DMA_DisableIT_HT(hw->dma, hw->ll_channel);
  LL_DMA_DisableIT_TE(hw->dma, hw->ll_channel);

  rulos_irq_state_t irq = hal_start_atomic();
  g_state[idx] = (dma_channel_state_t){0};
  hal_end_atomic(irq);
}

// ----------------------------------------------------------------------------
// IRQ dispatch
// ----------------------------------------------------------------------------

CCMRAM static void dispatch_channel_irq(int idx) {
  const dma_channel_hw_t *hw = &g_hw[idx];
  dma_channel_state_t *s = &g_state[idx];
  if (hw->dma == NULL || !s->allocated) {
    return;
  }

  if (ll_dma_is_active_flag_tc(hw->dma, hw->ll_channel)) {
    ll_dma_clear_flag_tc(hw->dma, hw->ll_channel);
    if (s->tc_callback) {
      s->tc_callback(s->user_data);
    }
  }

  if (ll_dma_is_active_flag_ht(hw->dma, hw->ll_channel)) {
    ll_dma_clear_flag_ht(hw->dma, hw->ll_channel);
    if (s->ht_callback) {
      s->ht_callback(s->user_data);
    }
  }

  if (ll_dma_is_active_flag_te(hw->dma, hw->ll_channel)) {
    ll_dma_clear_flag_te(hw->dma, hw->ll_channel);
    if (s->error_callback) {
      s->error_callback(s->user_data);
    }
  }
}

/*
 * Per-chip IRQ handlers. Each handler dispatches to one or more
 * channel slots in g_state[]/g_hw[]. On G4 every DMA channel has its
 * own dedicated IRQ, so handlers are one-to-one. On G0 many channels
 * share an IRQ line and the shared handler loops over every slot in
 * its group, calling dispatch_channel_irq() on each. The dispatcher
 * early-exits on channels that didn't fire, so calling it on idle
 * channels is essentially free (one load + one early return).
 */

#if defined(RULOS_ARM_stm32g4) || defined(RULOS_ARM_stm32f1) || \
    defined(RULOS_ARM_stm32f3)

#ifdef DMA1_Channel1_BASE
CCMRAM void DMA1_Channel1_IRQHandler(void) {
  dispatch_channel_irq(0);
}
#endif
#ifdef DMA1_Channel2_BASE
CCMRAM void DMA1_Channel2_IRQHandler(void) {
  dispatch_channel_irq(1);
}
#endif
#ifdef DMA1_Channel3_BASE
CCMRAM void DMA1_Channel3_IRQHandler(void) {
  dispatch_channel_irq(2);
}
#endif
#ifdef DMA1_Channel4_BASE
CCMRAM void DMA1_Channel4_IRQHandler(void) {
  dispatch_channel_irq(3);
}
#endif
#ifdef DMA1_Channel5_BASE
CCMRAM void DMA1_Channel5_IRQHandler(void) {
  dispatch_channel_irq(4);
}
#endif
#ifdef DMA1_Channel6_BASE
CCMRAM void DMA1_Channel6_IRQHandler(void) {
  dispatch_channel_irq(5);
}
#endif
#ifdef DMA1_Channel7_BASE
CCMRAM void DMA1_Channel7_IRQHandler(void) {
  dispatch_channel_irq(6);
}
#endif
#ifdef DMA1_Channel8_BASE
CCMRAM void DMA1_Channel8_IRQHandler(void) {
  dispatch_channel_irq(7);
}
#endif
#ifdef DMA2_Channel1_BASE
CCMRAM void DMA2_Channel1_IRQHandler(void) {
  dispatch_channel_irq(8);
}
#endif
#ifdef DMA2_Channel2_BASE
CCMRAM void DMA2_Channel2_IRQHandler(void) {
  dispatch_channel_irq(9);
}
#endif
#ifdef DMA2_Channel3_BASE
CCMRAM void DMA2_Channel3_IRQHandler(void) {
  dispatch_channel_irq(10);
}
#endif
#ifdef DMA2_Channel4_BASE
CCMRAM void DMA2_Channel4_IRQHandler(void) {
  dispatch_channel_irq(11);
}
#endif
#ifdef DMA2_Channel5_BASE
CCMRAM void DMA2_Channel5_IRQHandler(void) {
  dispatch_channel_irq(12);
}
#endif
#ifdef DMA2_Channel6_BASE
CCMRAM void DMA2_Channel6_IRQHandler(void) {
  dispatch_channel_irq(13);
}
#endif
#ifdef DMA2_Channel7_BASE
CCMRAM void DMA2_Channel7_IRQHandler(void) {
  dispatch_channel_irq(14);
}
#endif
#ifdef DMA2_Channel8_BASE
CCMRAM void DMA2_Channel8_IRQHandler(void) {
  dispatch_channel_irq(15);
}
#endif

#elif defined(RULOS_ARM_stm32g0)

// DMA1 Channel 1 has its own dedicated interrupt.
void DMA1_Channel1_IRQHandler(void) {
  dispatch_channel_irq(0);
}

// DMA1 Channels 2 and 3 share one interrupt line.
void DMA1_Channel2_3_IRQHandler(void) {
  dispatch_channel_irq(1);  // DMA1 Channel 2
  dispatch_channel_irq(2);  // DMA1 Channel 3
}

#if defined(STM32G0B1xx)
// DMA1 Ch4-7 and DMA2 Ch1-5 share a single interrupt, along with
// DMAMUX1 overrun. We ignore DMAMUX overrun entirely -- the dispatch
// loop just checks every possible channel.
void DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQHandler(void) {
  dispatch_channel_irq(3);   // DMA1 Channel 4
  dispatch_channel_irq(4);   // DMA1 Channel 5
  dispatch_channel_irq(5);   // DMA1 Channel 6
  dispatch_channel_irq(6);   // DMA1 Channel 7
  dispatch_channel_irq(8);   // DMA2 Channel 1
  dispatch_channel_irq(9);   // DMA2 Channel 2
  dispatch_channel_irq(10);  // DMA2 Channel 3
  dispatch_channel_irq(11);  // DMA2 Channel 4
  dispatch_channel_irq(12);  // DMA2 Channel 5
}
#else
// Smaller G0 variants (G030/G031) have only 5 DMA1 channels and no
// DMA2. The merged handler name differs accordingly.
void DMA1_Ch4_5_DMAMUX1_OVR_IRQHandler(void) {
  dispatch_channel_irq(3);  // DMA1 Channel 4
  dispatch_channel_irq(4);  // DMA1 Channel 5
}
#endif  // STM32G0B1xx

#elif defined(RULOS_ARM_stm32c0)

// C0 has 3 DMA channels. Ch1 is standalone, Ch2-3 share a line.
// The DMAMUX1 overrun IRQ is separate (and unused -- we don't enable
// DMAMUX sync-overrun interrupts anywhere, so that vector stays
// at the default weak handler).
void DMA1_Channel1_IRQHandler(void) {
  dispatch_channel_irq(0);
}
void DMA1_Channel2_3_IRQHandler(void) {
  dispatch_channel_irq(1);  // DMA1 Channel 2
  dispatch_channel_irq(2);  // DMA1 Channel 3
}

#else  // F0

// F0 has the same three-handler layout as G0 but different names
// (no DMAMUX, so no DMAMUX1_OVR suffix).
void DMA1_Channel1_IRQHandler(void) {
  dispatch_channel_irq(0);
}
void DMA1_Channel2_3_IRQHandler(void) {
  dispatch_channel_irq(1);  // DMA1 Channel 2
  dispatch_channel_irq(2);  // DMA1 Channel 3
}
void DMA1_Channel4_5_IRQHandler(void) {
  dispatch_channel_irq(3);  // DMA1 Channel 4
  dispatch_channel_irq(4);  // DMA1 Channel 5
}

#endif  // per-chip IRQ handlers

#endif  // CCR-based DMA families
