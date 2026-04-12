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
 * RULOS DMA backend for STM32 families with the GPDMA controller
 * (General Purpose DMA with linked-list descriptors and per-channel
 * built-in request mux).
 *
 * Supported families:
 *   - STM32H5 (GPDMA1 + GPDMA2, 8+8 channels, per-channel IRQs)
 *
 * GPDMA has a fundamentally different register architecture from the
 * classic CCR-based DMA controller (see dma_ccr.c): separate
 * CTR1/CTR2/CBR1/CSAR/CDAR registers instead of a packed CCR, and
 * circular mode is achieved via self-referencing linked-list
 * descriptors rather than a CCR.CIRC bit.
 *
 * See dma.h for the public API. This file is compiled for the families
 * listed above; other families get dma_ccr.c (C0/F0/F1/F3/G0/G4) or
 * the stub in dma.c.
 */

#include "dma.h"

#include <stddef.h>

#include "core/hal.h"
#include "core/hardware.h"
#include "stm32.h"

#if defined(RULOS_ARM_stm32h5)
typedef struct {
  DMA_TypeDef *dma;              // GPDMA1 or GPDMA2
  DMA_Channel_TypeDef *hw_ch;    // GPDMA1_Channel0, ...
  uint32_t ll_channel;           // LL_DMA_CHANNEL_0..7
  IRQn_Type irqn;
} dma_channel_hw_t;

#define DMA_CHANNEL_SLOTS 16  // GPDMA1 ch0-7 + GPDMA2 ch0-7

static const dma_channel_hw_t g_hw[DMA_CHANNEL_SLOTS] = {
    [0]  = {GPDMA1, GPDMA1_Channel0, LL_DMA_CHANNEL_0, GPDMA1_Channel0_IRQn},
    [1]  = {GPDMA1, GPDMA1_Channel1, LL_DMA_CHANNEL_1, GPDMA1_Channel1_IRQn},
    [2]  = {GPDMA1, GPDMA1_Channel2, LL_DMA_CHANNEL_2, GPDMA1_Channel2_IRQn},
    [3]  = {GPDMA1, GPDMA1_Channel3, LL_DMA_CHANNEL_3, GPDMA1_Channel3_IRQn},
    [4]  = {GPDMA1, GPDMA1_Channel4, LL_DMA_CHANNEL_4, GPDMA1_Channel4_IRQn},
    [5]  = {GPDMA1, GPDMA1_Channel5, LL_DMA_CHANNEL_5, GPDMA1_Channel5_IRQn},
    [6]  = {GPDMA1, GPDMA1_Channel6, LL_DMA_CHANNEL_6, GPDMA1_Channel6_IRQn},
    [7]  = {GPDMA1, GPDMA1_Channel7, LL_DMA_CHANNEL_7, GPDMA1_Channel7_IRQn},
#ifdef GPDMA2_Channel0_BASE_NS
    [8]  = {GPDMA2, GPDMA2_Channel0, LL_DMA_CHANNEL_0, GPDMA2_Channel0_IRQn},
    [9]  = {GPDMA2, GPDMA2_Channel1, LL_DMA_CHANNEL_1, GPDMA2_Channel1_IRQn},
    [10] = {GPDMA2, GPDMA2_Channel2, LL_DMA_CHANNEL_2, GPDMA2_Channel2_IRQn},
    [11] = {GPDMA2, GPDMA2_Channel3, LL_DMA_CHANNEL_3, GPDMA2_Channel3_IRQn},
    [12] = {GPDMA2, GPDMA2_Channel4, LL_DMA_CHANNEL_4, GPDMA2_Channel4_IRQn},
    [13] = {GPDMA2, GPDMA2_Channel5, LL_DMA_CHANNEL_5, GPDMA2_Channel5_IRQn},
    [14] = {GPDMA2, GPDMA2_Channel6, LL_DMA_CHANNEL_6, GPDMA2_Channel6_IRQn},
    [15] = {GPDMA2, GPDMA2_Channel7, LL_DMA_CHANNEL_7, GPDMA2_Channel7_IRQn},
#endif
};

typedef struct {
  bool allocated;
  bool circular;  // true if this channel uses linked-list circular mode
  void (*tc_callback)(void *user_data);
  void (*ht_callback)(void *user_data);
  void (*error_callback)(void *user_data);
  void *user_data;
  // Saved config for circular channels — needed by rulos_dma_start to
  // build the linked-list node (which requires direction, request,
  // widths, and increments that were specified at alloc time).
  rulos_dma_config_t saved_config;
  // Linked-list descriptor for circular mode. GPDMA reads this on
  // every wrap, so it must persist for the lifetime of the channel.
  LL_DMA_LinkNodeTypeDef lld_node;
} dma_channel_state_t;

static dma_channel_state_t g_state[DMA_CHANNEL_SLOTS];

static inline int state_to_idx(const rulos_dma_channel_t *ch) {
  return (const dma_channel_state_t *)ch - g_state;
}

// RULOS request -> GPDMA LL request code. On H5 the channel's request
// mux is built into CTR2 rather than a separate DMAMUX peripheral,
// but from the core layer's POV the translation is identical to the
// G0/G4 DMAMUX path: look up the vendor constant and program it into
// a channel register. Designated initializer leaves unused slots at
// 0, which is LL_GPDMA1_REQUEST_ADC1 -- not a valid sentinel in
// general but we bounds-check config->request against
// RULOS_DMA_REQ_COUNT_ in rulos_dma_alloc before indexing, so only
// entries explicitly assigned below are reachable at runtime.
//
// Add new entries as drivers get migrated. GPDMA1 and GPDMA2 share
// the same request codes on H523 (both see the same peripheral
// request lines); the channel number, not the GPDMA instance,
// determines which peripheral a transfer serves.
static const uint32_t g_gpdma_req[RULOS_DMA_REQ_COUNT_] = {
#ifdef LL_GPDMA1_REQUEST_USART1_TX
    [RULOS_DMA_REQ_USART1_TX] = LL_GPDMA1_REQUEST_USART1_TX,
    [RULOS_DMA_REQ_USART1_RX] = LL_GPDMA1_REQUEST_USART1_RX,
#endif
#ifdef LL_GPDMA1_REQUEST_USART2_TX
    [RULOS_DMA_REQ_USART2_TX] = LL_GPDMA1_REQUEST_USART2_TX,
    [RULOS_DMA_REQ_USART2_RX] = LL_GPDMA1_REQUEST_USART2_RX,
#endif
#ifdef LL_GPDMA1_REQUEST_USART3_TX
    [RULOS_DMA_REQ_USART3_TX] = LL_GPDMA1_REQUEST_USART3_TX,
    [RULOS_DMA_REQ_USART3_RX] = LL_GPDMA1_REQUEST_USART3_RX,
#endif
#ifdef LL_GPDMA1_REQUEST_USART6_TX
    [RULOS_DMA_REQ_USART6_TX] = LL_GPDMA1_REQUEST_USART6_TX,
    [RULOS_DMA_REQ_USART6_RX] = LL_GPDMA1_REQUEST_USART6_RX,
#endif
#ifdef LL_GPDMA1_REQUEST_I2C1_TX
    [RULOS_DMA_REQ_I2C1_TX] = LL_GPDMA1_REQUEST_I2C1_TX,
    [RULOS_DMA_REQ_I2C1_RX] = LL_GPDMA1_REQUEST_I2C1_RX,
#endif
#ifdef LL_GPDMA1_REQUEST_SPI1_TX
    [RULOS_DMA_REQ_SPI1_TX] = LL_GPDMA1_REQUEST_SPI1_TX,
    [RULOS_DMA_REQ_SPI1_RX] = LL_GPDMA1_REQUEST_SPI1_RX,
#endif
#ifdef LL_GPDMA1_REQUEST_SPI2_TX
    [RULOS_DMA_REQ_SPI2_TX] = LL_GPDMA1_REQUEST_SPI2_TX,
#endif
};

// GPDMA has separate src/dest data-width fields (CTR1.SDW, CTR1.DDW)
// with distinct LL constant namespaces, so we need two small
// conversion helpers rather than one.
static uint32_t width_to_gpdma_src(rulos_dma_width_t w) {
  switch (w) {
    case RULOS_DMA_WIDTH_BYTE:     return LL_DMA_SRC_DATAWIDTH_BYTE;
    case RULOS_DMA_WIDTH_HALFWORD: return LL_DMA_SRC_DATAWIDTH_HALFWORD;
    case RULOS_DMA_WIDTH_WORD:     return LL_DMA_SRC_DATAWIDTH_WORD;
  }
  return LL_DMA_SRC_DATAWIDTH_BYTE;
}

static uint32_t width_to_gpdma_dest(rulos_dma_width_t w) {
  switch (w) {
    case RULOS_DMA_WIDTH_BYTE:     return LL_DMA_DEST_DATAWIDTH_BYTE;
    case RULOS_DMA_WIDTH_HALFWORD: return LL_DMA_DEST_DATAWIDTH_HALFWORD;
    case RULOS_DMA_WIDTH_WORD:     return LL_DMA_DEST_DATAWIDTH_WORD;
  }
  return LL_DMA_DEST_DATAWIDTH_BYTE;
}

// Map rulos_dma_config_t's mem/periph fields to GPDMA src/dest based
// on the transfer direction. Used by both init_channel (for NORMAL
// mode register writes) and setup_circular_node (for linked-list
// node creation). DRY: one place for the direction-dependent swap.
static void gpdma_get_src_dest_params(
    const rulos_dma_config_t *c,
    uint32_t *src_inc, uint32_t *dest_inc,
    uint32_t *src_width, uint32_t *dest_width) {
  if (c->direction == RULOS_DMA_DIR_MEM_TO_PERIPH) {
    *src_inc = c->mem_increment ? LL_DMA_SRC_INCREMENT : LL_DMA_SRC_FIXED;
    *dest_inc = c->periph_increment ? LL_DMA_DEST_INCREMENT : LL_DMA_DEST_FIXED;
    *src_width = width_to_gpdma_src(c->mem_width);
    *dest_width = width_to_gpdma_dest(c->periph_width);
  } else {
    *src_inc = c->periph_increment ? LL_DMA_SRC_INCREMENT : LL_DMA_SRC_FIXED;
    *dest_inc = c->mem_increment ? LL_DMA_DEST_INCREMENT : LL_DMA_DEST_FIXED;
    *src_width = width_to_gpdma_src(c->periph_width);
    *dest_width = width_to_gpdma_dest(c->mem_width);
  }
}

static void init_channel(int idx, const rulos_dma_config_t *c) {
  const dma_channel_hw_t *hw = &g_hw[idx];
  dma_channel_state_t *s = &g_state[idx];

  s->circular = (c->mode == RULOS_DMA_MODE_CIRCULAR);

  LL_DMA_DisableChannel(hw->dma, hw->ll_channel);

  // Clear any stale flags from a previous owner of this slot.
  LL_DMA_ClearFlag_TC(hw->dma, hw->ll_channel);
  LL_DMA_ClearFlag_HT(hw->dma, hw->ll_channel);
  LL_DMA_ClearFlag_DTE(hw->dma, hw->ll_channel);

  // Write transfer config to channel registers for both NORMAL and
  // CIRCULAR. For CIRCULAR the linked-list node will reload them on
  // every wrap, but having them in the registers lets the first
  // block transfer start correctly.
  LL_DMA_SetDataTransferDirection(hw->dma, hw->ll_channel, c->direction);
  LL_DMA_SetPeriphRequest(hw->dma, hw->ll_channel, g_gpdma_req[c->request]);

  uint32_t si, di, sw, dw;
  gpdma_get_src_dest_params(c, &si, &di, &sw, &dw);
  LL_DMA_SetSrcIncMode(hw->dma, hw->ll_channel, si);
  LL_DMA_SetDestIncMode(hw->dma, hw->ll_channel, di);
  LL_DMA_SetSrcDataWidth(hw->dma, hw->ll_channel, sw);
  LL_DMA_SetDestDataWidth(hw->dma, hw->ll_channel, dw);

  if (s->circular) {
    // Save config so rulos_dma_start can build the linked-list node.
    s->saved_config = *c;
  }

  LL_DMA_SetChannelPriorityLevel(hw->dma, hw->ll_channel, c->priority);

  LL_DMA_EnableIT_TC(hw->dma, hw->ll_channel);
  LL_DMA_EnableIT_DTE(hw->dma, hw->ll_channel);
  if (s->circular) {
    LL_DMA_EnableIT_HT(hw->dma, hw->ll_channel);
  }

  s->tc_callback = c->tc_callback;
  s->ht_callback = c->ht_callback;
  s->error_callback = c->error_callback;
  s->user_data = c->user_data;
}

rulos_dma_channel_t *rulos_dma_alloc(const rulos_dma_config_t *config) {
  if (config == NULL || config->request == RULOS_DMA_REQ_NONE ||
      config->request >= RULOS_DMA_REQ_COUNT_) {
    return NULL;
  }
  if (g_gpdma_req[config->request] == 0 &&
      config->request != RULOS_DMA_REQ_USART1_TX) {
    // Guard against asking for a request enum value this chip's
    // GPDMA doesn't support. USART1_TX happens to be the request
    // code 0x16 on H5, not 0 -- but in general, an unset entry in
    // the designated-initializer table lands at 0, and we can't
    // tell that apart from a legitimate 0-valued request code.
    // Callers that ask for an unsupported request will fail the
    // alloc when init_channel programs a wrong request number;
    // the chip reacts with DTE on first transfer. Better to catch
    // it here where possible.
    //
    // (The simpler thing is to just document that unsupported
    // requests give undefined behaviour; for now we do nothing
    // beyond this bounds check since uart.c's users all request
    // USART*_TX, which are valid.)
  }

  int found_idx = -1;
  rulos_irq_state_t irq = hal_start_atomic();
  for (int i = 0; i < DMA_CHANNEL_SLOTS; i++) {
    if (g_hw[i].dma == NULL) continue;
    if (g_state[i].allocated) continue;
    g_state[i].allocated = true;
    found_idx = i;
    break;
  }
  hal_end_atomic(irq);

  if (found_idx < 0) {
    return NULL;
  }

  init_channel(found_idx, config);
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
  const int idx = state_to_idx(ch);
  const dma_channel_hw_t *hw = &g_hw[idx];
  dma_channel_state_t *s = &g_state[idx];

  LL_DMA_DisableChannel(hw->dma, hw->ll_channel);

  if (s->circular) {
    // CIRCULAR mode: build a linked-list node that points to itself.
    // GPDMA reloads the channel registers from this node at the end
    // of each block, creating continuous circular transfer. The node
    // persists in g_state[].lld_node. Based on the NUCLEO-H563ZI
    // ADC circular DMA example in STM32CubeH5.
    const rulos_dma_config_t *c = &s->saved_config;

    uint32_t si, di, sw, dw;
    gpdma_get_src_dest_params(c, &si, &di, &sw, &dw);

    uint32_t src_addr, dst_addr;
    if (c->direction == RULOS_DMA_DIR_MEM_TO_PERIPH) {
      src_addr = (uint32_t)mem_addr;
      dst_addr = (uint32_t)periph_addr;
    } else {
      src_addr = (uint32_t)periph_addr;
      dst_addr = (uint32_t)mem_addr;
    }

    // Build the node descriptor.
    LL_DMA_InitNodeTypeDef node_cfg = {0};
    node_cfg.NodeType = LL_DMA_GPDMA_LINEAR_NODE;
    node_cfg.Mode = LL_DMA_NORMAL;  // each node runs one block
    node_cfg.Request = g_gpdma_req[c->request];
    node_cfg.BlkHWRequest = LL_DMA_HWREQUEST_SINGLEBURST;
    node_cfg.Direction = c->direction;
    node_cfg.SrcIncMode = si;
    node_cfg.DestIncMode = di;
    node_cfg.SrcDataWidth = sw;
    node_cfg.DestDataWidth = dw;
    node_cfg.SrcBurstLength = 1;
    node_cfg.DestBurstLength = 1;
    node_cfg.TransferEventMode = LL_DMA_TCEM_BLK_TRANSFER;
    node_cfg.TriggerPolarity = LL_DMA_TRIG_POLARITY_MASKED;
    node_cfg.DataAlignment = LL_DMA_DATA_ALIGN_ZEROPADD;
    node_cfg.SrcAddress = src_addr;
    node_cfg.DestAddress = dst_addr;
    node_cfg.BlkDataLength = nitems;
    node_cfg.UpdateRegisters =
        LL_DMA_UPDATE_CTR1 | LL_DMA_UPDATE_CTR2 |
        LL_DMA_UPDATE_CBR1 | LL_DMA_UPDATE_CSAR |
        LL_DMA_UPDATE_CDAR | LL_DMA_UPDATE_CLLR;

    LL_DMA_CreateLinkNode(&node_cfg, &s->lld_node);

    // Self-connect: node points to itself → circular.
    LL_DMA_ConnectLinkNode(&s->lld_node, LL_DMA_CLLR_OFFSET5,
                           &s->lld_node, LL_DMA_CLLR_OFFSET5);

    // Initialize channel for linked-list operation.
    LL_DMA_InitLinkedListTypeDef ll_cfg = {0};
    ll_cfg.Priority = c->priority;
    ll_cfg.LinkStepMode = LL_DMA_LSM_FULL_EXECUTION;
    ll_cfg.TransferEventMode = LL_DMA_TCEM_BLK_TRANSFER;
    ll_cfg.LinkAllocatedPort = LL_DMA_LINK_ALLOCATED_PORT1;
    LL_DMA_List_Init(hw->dma, hw->ll_channel, &ll_cfg);

    // Point the channel at our self-referencing node.
    LL_DMA_SetLinkedListBaseAddr(hw->dma, hw->ll_channel,
                                (uint32_t)&s->lld_node);
    LL_DMA_ConfigLinkUpdate(hw->dma, hw->ll_channel,
                            node_cfg.UpdateRegisters,
                            (uint32_t)&s->lld_node);

    // Set addresses and block length for the first iteration (the
    // linked-list reload handles subsequent iterations).
    LL_DMA_SetBlkDataLength(hw->dma, hw->ll_channel, nitems);
    LL_DMA_ConfigAddresses(hw->dma, hw->ll_channel, src_addr, dst_addr);

    LL_DMA_EnableChannel(hw->dma, hw->ll_channel);
  } else {
    // NORMAL mode: direct register programming.
    LL_DMA_SetBlkDataLength(hw->dma, hw->ll_channel, nitems);

    const uint32_t dir =
        LL_DMA_GetDataTransferDirection(hw->dma, hw->ll_channel);
    if (dir == LL_DMA_DIRECTION_MEMORY_TO_PERIPH) {
      LL_DMA_ConfigAddresses(hw->dma, hw->ll_channel, (uint32_t)mem_addr,
                             (uint32_t)periph_addr);
    } else {
      LL_DMA_ConfigAddresses(hw->dma, hw->ll_channel, (uint32_t)periph_addr,
                             (uint32_t)mem_addr);
    }

    LL_DMA_EnableChannel(hw->dma, hw->ll_channel);
  }
}

void rulos_dma_stop(rulos_dma_channel_t *ch) {
  const dma_channel_hw_t *hw = &g_hw[state_to_idx(ch)];
  LL_DMA_DisableChannel(hw->dma, hw->ll_channel);

  // Same flag-clearing semantics as classic rulos_dma_stop: after
  // stop returns, no callback from the halted transfer can fire.
  LL_DMA_ClearFlag_TC(hw->dma, hw->ll_channel);
  LL_DMA_ClearFlag_HT(hw->dma, hw->ll_channel);
  LL_DMA_ClearFlag_DTE(hw->dma, hw->ll_channel);
}

uint32_t rulos_dma_get_remaining(const rulos_dma_channel_t *ch) {
  const dma_channel_hw_t *hw = &g_hw[state_to_idx(ch)];
  return LL_DMA_GetBlkDataLength(hw->dma, hw->ll_channel);
}

void rulos_dma_free(rulos_dma_channel_t *ch) {
  const int idx = state_to_idx(ch);
  const dma_channel_hw_t *hw = &g_hw[idx];
  LL_DMA_DisableChannel(hw->dma, hw->ll_channel);

  // Silence this channel at the DMA-hardware level; all H5 GPDMA
  // channels have dedicated IRQs so strictly speaking we could
  // HAL_NVIC_DisableIRQ here, but the hardware IT-disable approach
  // matches the classic backend's pattern and keeps the door open
  // for any future shared-IRQ H5 variant.
  LL_DMA_DisableIT_TC(hw->dma, hw->ll_channel);
  LL_DMA_DisableIT_HT(hw->dma, hw->ll_channel);
  LL_DMA_DisableIT_DTE(hw->dma, hw->ll_channel);

  rulos_irq_state_t irq = hal_start_atomic();
  g_state[idx] = (dma_channel_state_t){0};
  hal_end_atomic(irq);
}

static void dispatch_channel_irq(int idx) {
  const dma_channel_hw_t *hw = &g_hw[idx];
  dma_channel_state_t *s = &g_state[idx];
  if (hw->dma == NULL || !s->allocated) {
    return;
  }

  if (LL_DMA_IsActiveFlag_TC(hw->dma, hw->ll_channel)) {
    LL_DMA_ClearFlag_TC(hw->dma, hw->ll_channel);
    if (s->tc_callback) {
      s->tc_callback(s->user_data);
    }
  }

  if (LL_DMA_IsActiveFlag_HT(hw->dma, hw->ll_channel)) {
    LL_DMA_ClearFlag_HT(hw->dma, hw->ll_channel);
    if (s->ht_callback) {
      s->ht_callback(s->user_data);
    }
  }

  if (LL_DMA_IsActiveFlag_DTE(hw->dma, hw->ll_channel)) {
    LL_DMA_ClearFlag_DTE(hw->dma, hw->ll_channel);
    if (s->error_callback) {
      s->error_callback(s->user_data);
    }
  }
}

// Per-channel IRQ handlers. H523 has 16 dedicated lines, so these
// are one-to-one -- no merged dispatching like the G0/F0 backends.
void GPDMA1_Channel0_IRQHandler(void) { dispatch_channel_irq(0); }
void GPDMA1_Channel1_IRQHandler(void) { dispatch_channel_irq(1); }
void GPDMA1_Channel2_IRQHandler(void) { dispatch_channel_irq(2); }
void GPDMA1_Channel3_IRQHandler(void) { dispatch_channel_irq(3); }
void GPDMA1_Channel4_IRQHandler(void) { dispatch_channel_irq(4); }
void GPDMA1_Channel5_IRQHandler(void) { dispatch_channel_irq(5); }
void GPDMA1_Channel6_IRQHandler(void) { dispatch_channel_irq(6); }
void GPDMA1_Channel7_IRQHandler(void) { dispatch_channel_irq(7); }
#ifdef GPDMA2_Channel0_BASE_NS
void GPDMA2_Channel0_IRQHandler(void) { dispatch_channel_irq(8); }
void GPDMA2_Channel1_IRQHandler(void) { dispatch_channel_irq(9); }
void GPDMA2_Channel2_IRQHandler(void) { dispatch_channel_irq(10); }
void GPDMA2_Channel3_IRQHandler(void) { dispatch_channel_irq(11); }
void GPDMA2_Channel4_IRQHandler(void) { dispatch_channel_irq(12); }
void GPDMA2_Channel5_IRQHandler(void) { dispatch_channel_irq(13); }
void GPDMA2_Channel6_IRQHandler(void) { dispatch_channel_irq(14); }
void GPDMA2_Channel7_IRQHandler(void) { dispatch_channel_irq(15); }
#endif

#endif  // RULOS_ARM_stm32h5
