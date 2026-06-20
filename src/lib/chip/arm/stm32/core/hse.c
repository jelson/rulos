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

// Weak default for the NMI first-refusal hook (see hse.h): with no
// other module linked, nothing claims the NMI, so every NMI is the
// CSS/HSE failure path.
__attribute__((weak)) bool rulos_nmi_claimed(void) {
  return false;
}

// NMI handler, owned here so every HSE app inherits CSS handling just by
// linking this module. It is defined STRONG (not weak): the startup file
// already provides a weak NMI_Handler aliased to the infinite-loop
// Default_Handler, and a second weak definition here would not reliably
// win -- the linker is free to keep the startup alias, leaving CSS armed
// but unhandled, so an HSE glitch would wedge the chip in the default
// loop until reset. A strong definition overrides the startup alias
// unconditionally. Other NMI sources are consumed via rulos_nmi_claimed,
// not by redefining this handler.
void NMI_Handler(void) {
  if (rulos_nmi_claimed()) {
    return;
  }
  HAL_RCC_NMI_IRQHandler();
}

// HAL_RCC_NMI_IRQHandler dispatches CSS failures here. Set the failure
// flag so apps polling for HSE health notice.
void HAL_RCC_CSSCallback(void) {
  g_rulos_hse_failed = true;
}

void rulos_hse_try_pll(RCC_OscInitTypeDef *osc, rulos_hsi_pll_config_fn hsi_fallback) {
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
