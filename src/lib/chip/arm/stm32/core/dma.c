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
 * RULOS DMA abstraction layer implementation.
 *
 * See dma.h for API docs. Per-family backends live in the #if blocks
 * below. Phase 1 of the refactor ships only the G4 backend; G0 / F0 /
 * F1 / F3 / H5 / C0 backends land in subsequent phases. In the meantime
 * the non-G4 stubs let those families' nulltest builds keep compiling
 * while their peripheral drivers still use the old hardcoded paths.
 */

#include "dma.h"

#include <stddef.h>

#include "core/hal.h"
#include "stm32.h"

// ============================================================================
// STM32G4 backend
// ============================================================================
#if defined(RULOS_ARM_stm32g4)

#include "stm32g4xx_ll_dma.h"

/*
 * Per-channel state. The array is sized to cover all 16 possible G4
 * channels (DMA1 channels 1..8, DMA2 channels 1..8). Variants like
 * STM32G431 that only expose 6 channels per DMA leave the upper slots
 * zero-initialized; the allocator skips entries whose `dma` pointer is
 * NULL.
 *
 * The per-channel IRQ handlers further down in this file hardcode the
 * array index they manage, so the designated initializer positions are
 * load-bearing:
 *   [0..7]  = DMA1 channels 1..8
 *   [8..15] = DMA2 channels 1..8
 */
typedef struct {
  DMA_TypeDef *dma;              // DMA1 or DMA2 (NULL = slot not populated)
  DMA_Channel_TypeDef *hw_ch;    // DMA1_Channel1, ...
  uint32_t ll_channel;           // LL_DMA_CHANNEL_1..8 (values 0..7)
  IRQn_Type irqn;
  bool allocated;
  rulos_dma_config_t config;
} dma_channel_state_t;

#define DMA_CHANNEL_SLOTS 16

static dma_channel_state_t g_channels[DMA_CHANNEL_SLOTS] = {
#ifdef DMA1_Channel1_BASE
    [0] = {.dma = DMA1,
           .hw_ch = DMA1_Channel1,
           .ll_channel = LL_DMA_CHANNEL_1,
           .irqn = DMA1_Channel1_IRQn},
#endif
#ifdef DMA1_Channel2_BASE
    [1] = {.dma = DMA1,
           .hw_ch = DMA1_Channel2,
           .ll_channel = LL_DMA_CHANNEL_2,
           .irqn = DMA1_Channel2_IRQn},
#endif
#ifdef DMA1_Channel3_BASE
    [2] = {.dma = DMA1,
           .hw_ch = DMA1_Channel3,
           .ll_channel = LL_DMA_CHANNEL_3,
           .irqn = DMA1_Channel3_IRQn},
#endif
#ifdef DMA1_Channel4_BASE
    [3] = {.dma = DMA1,
           .hw_ch = DMA1_Channel4,
           .ll_channel = LL_DMA_CHANNEL_4,
           .irqn = DMA1_Channel4_IRQn},
#endif
#ifdef DMA1_Channel5_BASE
    [4] = {.dma = DMA1,
           .hw_ch = DMA1_Channel5,
           .ll_channel = LL_DMA_CHANNEL_5,
           .irqn = DMA1_Channel5_IRQn},
#endif
#ifdef DMA1_Channel6_BASE
    [5] = {.dma = DMA1,
           .hw_ch = DMA1_Channel6,
           .ll_channel = LL_DMA_CHANNEL_6,
           .irqn = DMA1_Channel6_IRQn},
#endif
#ifdef DMA1_Channel7_BASE
    [6] = {.dma = DMA1,
           .hw_ch = DMA1_Channel7,
           .ll_channel = LL_DMA_CHANNEL_7,
           .irqn = DMA1_Channel7_IRQn},
#endif
#ifdef DMA1_Channel8_BASE
    [7] = {.dma = DMA1,
           .hw_ch = DMA1_Channel8,
           .ll_channel = LL_DMA_CHANNEL_8,
           .irqn = DMA1_Channel8_IRQn},
#endif
#ifdef DMA2_Channel1_BASE
    [8] = {.dma = DMA2,
           .hw_ch = DMA2_Channel1,
           .ll_channel = LL_DMA_CHANNEL_1,
           .irqn = DMA2_Channel1_IRQn},
#endif
#ifdef DMA2_Channel2_BASE
    [9] = {.dma = DMA2,
           .hw_ch = DMA2_Channel2,
           .ll_channel = LL_DMA_CHANNEL_2,
           .irqn = DMA2_Channel2_IRQn},
#endif
#ifdef DMA2_Channel3_BASE
    [10] = {.dma = DMA2,
            .hw_ch = DMA2_Channel3,
            .ll_channel = LL_DMA_CHANNEL_3,
            .irqn = DMA2_Channel3_IRQn},
#endif
#ifdef DMA2_Channel4_BASE
    [11] = {.dma = DMA2,
            .hw_ch = DMA2_Channel4,
            .ll_channel = LL_DMA_CHANNEL_4,
            .irqn = DMA2_Channel4_IRQn},
#endif
#ifdef DMA2_Channel5_BASE
    [12] = {.dma = DMA2,
            .hw_ch = DMA2_Channel5,
            .ll_channel = LL_DMA_CHANNEL_5,
            .irqn = DMA2_Channel5_IRQn},
#endif
#ifdef DMA2_Channel6_BASE
    [13] = {.dma = DMA2,
            .hw_ch = DMA2_Channel6,
            .ll_channel = LL_DMA_CHANNEL_6,
            .irqn = DMA2_Channel6_IRQn},
#endif
#ifdef DMA2_Channel7_BASE
    [14] = {.dma = DMA2,
            .hw_ch = DMA2_Channel7,
            .ll_channel = LL_DMA_CHANNEL_7,
            .irqn = DMA2_Channel7_IRQn},
#endif
#ifdef DMA2_Channel8_BASE
    [15] = {.dma = DMA2,
            .hw_ch = DMA2_Channel8,
            .ll_channel = LL_DMA_CHANNEL_8,
            .irqn = DMA2_Channel8_IRQn},
#endif
};

/*
 * RULOS request enum -> G4 DMAMUX request code. Designated initializers
 * leave unused slots at 0 (= LL_DMAMUX_REQ_MEM2MEM, safe sentinel).
 */
static const uint32_t g_dmamux_req[RULOS_DMA_REQ_COUNT_] = {
    [RULOS_DMA_REQ_USART1_TX] = LL_DMAMUX_REQ_USART1_TX,
    [RULOS_DMA_REQ_USART1_RX] = LL_DMAMUX_REQ_USART1_RX,
    [RULOS_DMA_REQ_USART2_TX] = LL_DMAMUX_REQ_USART2_TX,
    [RULOS_DMA_REQ_USART2_RX] = LL_DMAMUX_REQ_USART2_RX,
    [RULOS_DMA_REQ_USART3_TX] = LL_DMAMUX_REQ_USART3_TX,
    [RULOS_DMA_REQ_USART3_RX] = LL_DMAMUX_REQ_USART3_RX,
#ifdef LL_DMAMUX_REQ_UART4_TX
    [RULOS_DMA_REQ_USART4_TX] = LL_DMAMUX_REQ_UART4_TX,
    [RULOS_DMA_REQ_USART4_RX] = LL_DMAMUX_REQ_UART4_RX,
#endif
#ifdef LL_DMAMUX_REQ_UART5_TX
    [RULOS_DMA_REQ_USART5_TX] = LL_DMAMUX_REQ_UART5_TX,
    [RULOS_DMA_REQ_USART5_RX] = LL_DMAMUX_REQ_UART5_RX,
#endif
    [RULOS_DMA_REQ_I2C1_TX] = LL_DMAMUX_REQ_I2C1_TX,
    [RULOS_DMA_REQ_I2C1_RX] = LL_DMAMUX_REQ_I2C1_RX,
    [RULOS_DMA_REQ_SPI1_TX] = LL_DMAMUX_REQ_SPI1_TX,
    [RULOS_DMA_REQ_SPI1_RX] = LL_DMAMUX_REQ_SPI1_RX,
    [RULOS_DMA_REQ_SPI2_TX] = LL_DMAMUX_REQ_SPI2_TX,
    [RULOS_DMA_REQ_TIM2_CH1] = LL_DMAMUX_REQ_TIM2_CH1,
    [RULOS_DMA_REQ_TIM2_CH2] = LL_DMAMUX_REQ_TIM2_CH2,
};

// ----------------------------------------------------------------------------
// Enum -> LL constant translation helpers
// ----------------------------------------------------------------------------

static uint32_t direction_to_ll(rulos_dma_direction_t d) {
  switch (d) {
    case RULOS_DMA_DIR_PERIPH_TO_MEM:
      return LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
    case RULOS_DMA_DIR_MEM_TO_PERIPH:
      return LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    case RULOS_DMA_DIR_MEM_TO_MEM:
      return LL_DMA_DIRECTION_MEMORY_TO_MEMORY;
  }
  return LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
}

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

static uint32_t mode_to_ll(rulos_dma_mode_t m) {
  return (m == RULOS_DMA_MODE_CIRCULAR) ? LL_DMA_MODE_CIRCULAR
                                        : LL_DMA_MODE_NORMAL;
}

static uint32_t priority_to_ll(uint8_t p) {
  switch (p) {
    case 0:
      return LL_DMA_PRIORITY_LOW;
    case 1:
      return LL_DMA_PRIORITY_MEDIUM;
    case 2:
      return LL_DMA_PRIORITY_HIGH;
    default:
      return LL_DMA_PRIORITY_VERYHIGH;
  }
}

// ----------------------------------------------------------------------------
// Private TE flag dispatch helpers (the LL_DMA_*_TE variants aren't wrapped
// into channel-indexed helpers by stm32g4xx_ll_dma.h; we do it here).
// ----------------------------------------------------------------------------

static bool ll_dma_is_active_flag_te(DMA_TypeDef *dma, uint32_t ch) {
  switch (ch) {
    case LL_DMA_CHANNEL_1:
      return LL_DMA_IsActiveFlag_TE1(dma);
    case LL_DMA_CHANNEL_2:
      return LL_DMA_IsActiveFlag_TE2(dma);
    case LL_DMA_CHANNEL_3:
      return LL_DMA_IsActiveFlag_TE3(dma);
    case LL_DMA_CHANNEL_4:
      return LL_DMA_IsActiveFlag_TE4(dma);
    case LL_DMA_CHANNEL_5:
      return LL_DMA_IsActiveFlag_TE5(dma);
    case LL_DMA_CHANNEL_6:
      return LL_DMA_IsActiveFlag_TE6(dma);
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

static void ll_dma_clear_flag_te(DMA_TypeDef *dma, uint32_t ch) {
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
    case LL_DMA_CHANNEL_4:
      LL_DMA_ClearFlag_TE4(dma);
      break;
    case LL_DMA_CHANNEL_5:
      LL_DMA_ClearFlag_TE5(dma);
      break;
    case LL_DMA_CHANNEL_6:
      LL_DMA_ClearFlag_TE6(dma);
      break;
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
// ----------------------------------------------------------------------------

static void init_channel(dma_channel_state_t *s) {
  const rulos_dma_config_t *c = &s->config;

  LL_DMA_DisableChannel(s->dma, s->ll_channel);

  // Build the CCR configuration word and apply it in one shot.
  uint32_t cfg =
      direction_to_ll(c->direction) | mode_to_ll(c->mode) |
      (c->periph_increment ? LL_DMA_PERIPH_INCREMENT : LL_DMA_PERIPH_NOINCREMENT) |
      (c->mem_increment ? LL_DMA_MEMORY_INCREMENT : LL_DMA_MEMORY_NOINCREMENT) |
      periph_width_to_ll(c->periph_width) | mem_width_to_ll(c->mem_width) |
      priority_to_ll(c->priority);
  LL_DMA_ConfigTransfer(s->dma, s->ll_channel, cfg);

  // Route the peripheral request through DMAMUX.
  LL_DMA_SetPeriphRequest(s->dma, s->ll_channel, g_dmamux_req[c->request]);

  // Enable the interrupts corresponding to the callbacks we'll dispatch.
  LL_DMA_EnableIT_TC(s->dma, s->ll_channel);
  LL_DMA_EnableIT_TE(s->dma, s->ll_channel);
  if (c->mode == RULOS_DMA_MODE_CIRCULAR) {
    LL_DMA_EnableIT_HT(s->dma, s->ll_channel);
  }
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

rulos_dma_channel_t *rulos_dma_alloc(const rulos_dma_config_t *config) {
  if (config == NULL || config->request == RULOS_DMA_REQ_NONE ||
      config->request >= RULOS_DMA_REQ_COUNT_) {
    return NULL;
  }
  if (g_dmamux_req[config->request] == 0 &&
      config->request != RULOS_DMA_REQ_NONE) {
    // The request enum value has no DMAMUX mapping on this family.
    return NULL;
  }

  dma_channel_state_t *found = NULL;
  rulos_irq_state_t irq = hal_start_atomic();
  for (size_t i = 0; i < DMA_CHANNEL_SLOTS; i++) {
    if (g_channels[i].dma == NULL) continue;  // slot not populated
    if (g_channels[i].allocated) continue;
    g_channels[i].allocated = true;
    found = &g_channels[i];
    break;
  }
  hal_end_atomic(irq);

  if (found == NULL) {
    return NULL;
  }

  found->config = *config;
  init_channel(found);

  // Priority 1 is the same priority RULOS already used for its DMA IRQs
  // (see the old uart.c init path). SysTick runs at the lowest priority
  // (15), so DMA ISRs can safely call schedule_now() via the RULOS
  // scheduler's ISR-safe path.
  HAL_NVIC_SetPriority(found->irqn, 1, 0);
  HAL_NVIC_EnableIRQ(found->irqn);

  return (rulos_dma_channel_t *)found;
}

void rulos_dma_reconfigure(rulos_dma_channel_t *ch,
                           const rulos_dma_config_t *new_config) {
  dma_channel_state_t *s = (dma_channel_state_t *)ch;
  s->config = *new_config;
  init_channel(s);
}

void rulos_dma_start(rulos_dma_channel_t *ch, volatile void *periph_addr,
                     void *mem_addr, uint32_t nitems) {
  dma_channel_state_t *s = (dma_channel_state_t *)ch;

  LL_DMA_DisableChannel(s->dma, s->ll_channel);
  LL_DMA_SetDataLength(s->dma, s->ll_channel, nitems);

  if (s->config.direction == RULOS_DMA_DIR_MEM_TO_PERIPH) {
    LL_DMA_ConfigAddresses(s->dma, s->ll_channel, (uint32_t)mem_addr,
                           (uint32_t)periph_addr,
                           LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
  } else {
    LL_DMA_ConfigAddresses(s->dma, s->ll_channel, (uint32_t)periph_addr,
                           (uint32_t)mem_addr,
                           LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
  }

  LL_DMA_EnableChannel(s->dma, s->ll_channel);
}

void rulos_dma_stop(rulos_dma_channel_t *ch) {
  dma_channel_state_t *s = (dma_channel_state_t *)ch;
  LL_DMA_DisableChannel(s->dma, s->ll_channel);
}

uint32_t rulos_dma_get_remaining(const rulos_dma_channel_t *ch) {
  const dma_channel_state_t *s = (const dma_channel_state_t *)ch;
  return LL_DMA_GetDataLength(s->dma, s->ll_channel);
}

void rulos_dma_free(rulos_dma_channel_t *ch) {
  dma_channel_state_t *s = (dma_channel_state_t *)ch;
  LL_DMA_DisableChannel(s->dma, s->ll_channel);
  HAL_NVIC_DisableIRQ(s->irqn);

  rulos_irq_state_t irq = hal_start_atomic();
  s->allocated = false;
  // Leave the other fields as-is; alloc will overwrite them.
  hal_end_atomic(irq);
}

// ----------------------------------------------------------------------------
// IRQ dispatch
// ----------------------------------------------------------------------------

static inline void dispatch_channel_irq(dma_channel_state_t *s) {
  if (s->dma == NULL || !s->allocated) {
    return;
  }

  if (LL_DMA_IsActiveFlag_TC(s->dma, s->ll_channel)) {
    LL_DMA_ClearFlag_TC(s->dma, s->ll_channel);
    if (s->config.tc_callback) {
      s->config.tc_callback(s->config.user_data);
    }
  }

  if (LL_DMA_IsActiveFlag_HT(s->dma, s->ll_channel)) {
    LL_DMA_ClearFlag_HT(s->dma, s->ll_channel);
    if (s->config.ht_callback) {
      s->config.ht_callback(s->config.user_data);
    }
  }

  if (ll_dma_is_active_flag_te(s->dma, s->ll_channel)) {
    ll_dma_clear_flag_te(s->dma, s->ll_channel);
    if (s->config.error_callback) {
      s->config.error_callback(s->config.user_data);
    }
  }
}

/*
 * Per-channel IRQ handlers. Each handler hardcodes the index into
 * g_channels[] it manages -- see the channel layout comment above the
 * array definition.
 *
 * DMA1_Channel3_IRQHandler and DMA1_Channel4_IRQHandler are NOT defined
 * here during the refactor: timestamper.c still owns them via its
 * pre-refactor code path and defines them as strong symbols. Phase 2
 * (timestamper migration) deletes timestamper's handlers and the
 * matching `#if 0` blocks below are enabled.
 */

#ifdef DMA1_Channel1_BASE
void DMA1_Channel1_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[0]);
}
#endif
#ifdef DMA1_Channel2_BASE
void DMA1_Channel2_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[1]);
}
#endif
#if 0  // Phase 2: timestamper migration enables this
#ifdef DMA1_Channel3_BASE
void DMA1_Channel3_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[2]);
}
#endif
#ifdef DMA1_Channel4_BASE
void DMA1_Channel4_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[3]);
}
#endif
#endif  // Phase 2 gate
#ifdef DMA1_Channel5_BASE
void DMA1_Channel5_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[4]);
}
#endif
#ifdef DMA1_Channel6_BASE
void DMA1_Channel6_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[5]);
}
#endif
#ifdef DMA1_Channel7_BASE
void DMA1_Channel7_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[6]);
}
#endif
#ifdef DMA1_Channel8_BASE
void DMA1_Channel8_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[7]);
}
#endif
#ifdef DMA2_Channel1_BASE
void DMA2_Channel1_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[8]);
}
#endif
#ifdef DMA2_Channel2_BASE
void DMA2_Channel2_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[9]);
}
#endif
#ifdef DMA2_Channel3_BASE
void DMA2_Channel3_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[10]);
}
#endif
#ifdef DMA2_Channel4_BASE
void DMA2_Channel4_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[11]);
}
#endif
#ifdef DMA2_Channel5_BASE
void DMA2_Channel5_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[12]);
}
#endif
#ifdef DMA2_Channel6_BASE
void DMA2_Channel6_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[13]);
}
#endif
#ifdef DMA2_Channel7_BASE
void DMA2_Channel7_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[14]);
}
#endif
#ifdef DMA2_Channel8_BASE
void DMA2_Channel8_IRQHandler(void) {
  dispatch_channel_irq(&g_channels[15]);
}
#endif

// ============================================================================
// Stub backend (non-G4 families for Phase 1 of the DMA refactor).
//
// These families still drive DMA via the old hardcoded paths in their
// peripheral drivers. The stub lets any nulltest / test app that doesn't
// call the new API keep compiling. When each family's backend lands in
// a later phase it will replace the stub.
// ============================================================================
#else

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

#endif  // backend selection
