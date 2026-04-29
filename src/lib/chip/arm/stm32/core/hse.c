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

#include "hse.h"

#ifdef RULOS_USE_HSE

bool g_rulos_hse_failed = false;

// Default NMI handler. The startup file defines NMI_Handler as a weak
// alias to Default_Handler. This weak override wires CSS-driven HSE
// failure into the HAL; can itself be overridden if an app needs
// custom NMI behavior.
__attribute__((weak)) void NMI_Handler(void) {
  HAL_RCC_NMI_IRQHandler();
}

// HAL_RCC_NMI_IRQHandler dispatches CSS failures here. Set the failure
// flag so apps polling for HSE health notice.
void HAL_RCC_CSSCallback(void) {
  g_rulos_hse_failed = true;
}

void rulos_hse_try_pll(RCC_OscInitTypeDef *osc,
                       rulos_hsi_pll_config_fn hsi_fallback) {
  if (HAL_RCC_OscConfig(osc) == HAL_OK) {
    // HSE locked. Enable CSS so a mid-stream HSE failure fires NMI.
    HAL_RCC_EnableCSS();
    return;
  }

  // HSE failed to start. Flag the failure and fall back to HSI so the
  // chip can still boot enough to surface the error. Apps that demand
  // HSE accuracy will see g_rulos_hse_failed and refuse to operate.
  g_rulos_hse_failed = true;
  *osc = (RCC_OscInitTypeDef){0};
  hsi_fallback(osc);
  if (HAL_RCC_OscConfig(osc) != HAL_OK) {
    __builtin_trap();
  }
}

#endif  // RULOS_USE_HSE
