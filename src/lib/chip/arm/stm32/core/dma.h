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
 * hardware.c: These functions are only needed for physical display hardware.
 *
 * This file is not compiled by the simulator.
 */

#include "core/hardware.h"
#include "stm32.h"

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
