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

#include <stdbool.h>

#include "core/hardware.h"
#include "stm32.h"

// Dynamic registration of DMA channels for chips that have a DMA mux
// (stm32g0/stm32g4). The assigned DMA instance and channel number are written
// into the output arguments. The provided Callback is called (with context
// pointer) when an interrupt for the assigned DMA channel is received.
typedef void (*dma_callback_t)(void *context);

void allocate_dma_channel(dma_callback_t dma_callback, void *context,
                          DMA_TypeDef **DMAx, int *channel_index);

bool LL_DMA_IsActiveFlag_TC(DMA_TypeDef *DMAx, int channel_index);
void LL_DMA_ClearFlag_TC(DMA_TypeDef *DMAx, int channel_index);
bool LL_DMA_IsActiveFlag_HT(DMA_TypeDef *DMAx, int channel_index);
void LL_DMA_ClearFlag_HT(DMA_TypeDef *DMAx, int channel_index);
