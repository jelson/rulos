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

#if defined(RULOS_ARM_stm32g0) || defined(RULOS_ARM_stm32g4)

#include "core/dma.h"

#include <stdbool.h>
#include <stdint.h>

#include "core/rulos.h"
#include "stm32.h"

#ifdef DMA2
#define MAX_DMA_CHANNELS 16
#else
#define MAX_DMA_CHANNELS 8
#endif

typedef struct {
  DMA_TypeDef *DMAx;
  int channel_index;
  dma_callback_t callback;
  void *context;
} dma_config_t;

static dma_config_t dma_configs[MAX_DMA_CHANNELS] = {};

dma_config_t *channel_config(DMA_TypeDef *DMAx, int channel_index) {
#if defined(DMA2)
  if (DMAx == DMA2) {
    channel_index += 8;
  }
#endif
  return &dma_configs[channel_index];
}

void allocate_dma_channel(dma_callback_t dma_callback, void *context,
                          DMA_TypeDef **DMAx, int *channel_index) {
  // must provide a callback
  assert(dma_callback != NULL);

  // try to find an unallocated channel
  dma_config_t *dc = NULL;
  for (int i = 0; i < MAX_DMA_CHANNELS; i++) {
    // check 1) this is a valid channel; 2) it has not been allocated already
    if (dma_configs[i].DMAx != NULL && dma_configs[i].callback == NULL) {
      dc = &dma_configs[i];
      break;
    }
  }

  // no slot found? assert
  assert(dc != NULL);

  // store callback info, and provide channel info to caller
  dc->callback = dma_callback;
  dc->context = context;
  *DMAx = dc->DMAx;
  *channel_index = dc->channel_index;
}

// just used during initialization
static void add_dma_channel(DMA_TypeDef *dmax, int channel_index) {
  dma_config_t *dc = channel_config(dmax, channel_index);
  dc->DMAx = dmax;
  dc->channel_index = channel_index;
}

void dynamic_dma_init() {
#if defined(DMA1_Channel1)
  add_dma_channel(DMA1, LL_DMA_CHANNEL_1);
#endif
#if defined(DMA1_Channel2)
  add_dma_channel(DMA1, LL_DMA_CHANNEL_2);
#endif
#if defined(DMA1_Channel3)
  add_dma_channel(DMA1, LL_DMA_CHANNEL_3);
#endif
#if defined(DMA1_Channel4)
  add_dma_channel(DMA1, LL_DMA_CHANNEL_4);
#endif
#if defined(DMA1_Channel5)
  add_dma_channel(DMA1, LL_DMA_CHANNEL_5);
#endif
#if defined(DMA1_Channel6)
  add_dma_channel(DMA1, LL_DMA_CHANNEL_6);
#endif
#if defined(DMA1_Channel7)
  add_dma_channel(DMA1, LL_DMA_CHANNEL_7);
#endif
#if defined(DMA1_Channel8)
  add_dma_channel(DMA1, LL_DMA_CHANNEL_8);
#endif
#if defined(DMA2_Channel1)
  add_dma_channel(DMA2, LL_DMA_CHANNEL_1);
#endif
#if defined(DMA2_Channel2)
  add_dma_channel(DMA2, LL_DMA_CHANNEL_2);
#endif
#if defined(DMA2_Channel3)
  add_dma_channel(DMA2, LL_DMA_CHANNEL_3);
#endif
#if defined(DMA2_Channel4)
  add_dma_channel(DMA2, LL_DMA_CHANNEL_4);
#endif
#if defined(DMA2_Channel5)
  add_dma_channel(DMA2, LL_DMA_CHANNEL_5);
#endif
#if defined(DMA2_Channel6)
  add_dma_channel(DMA2, LL_DMA_CHANNEL_6);
#endif
#if defined(DMA2_Channel7)
  add_dma_channel(DMA2, LL_DMA_CHANNEL_7);
#endif
#if defined(DMA2_Channel8)
  add_dma_channel(DMA2, LL_DMA_CHANNEL_8);
#endif
}

///////////

bool LL_DMA_IsActiveFlag_TC(DMA_TypeDef *DMAx, int channel_index) {
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

void LL_DMA_ClearFlag_TC(DMA_TypeDef *DMAx, int channel_index) {
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

bool LL_DMA_IsActiveFlag_HT(DMA_TypeDef *DMAx, int channel_index) {
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

void LL_DMA_ClearFlag_HT(DMA_TypeDef *DMAx, int channel_index) {
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

#endif  // defined(RULOS_ARM_stm32g0) || defined(RULOS_ARM_STM32g4)
