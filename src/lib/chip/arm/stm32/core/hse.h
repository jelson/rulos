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

#pragma once

// Shared HSE-failure plumbing for STM32 families that support the
// optional HSE oscillator path. Compiled in only when an app sets
// -DRULOS_USE_HSE.
//
// The actual HSE -> PLL configuration is family-specific (G4 uses
// RCC_PLLM_DIVn macros, H5 uses raw integers, and the surrounding PLL
// fields differ) and lives in each family's SystemClock_Config in
// stm32-hardware.c. What's common across families and lives here:
//
//   - g_rulos_hse_failed: a single global that records HSE failure,
//     either a boot-time start failure or a runtime CSS-detected loss.
//   - The NMI_Handler weak override and HAL_RCC_CSSCallback that wire
//     CSS into the failure flag.
//   - rulos_hse_try_pll: the try-HSE / fall-back-to-HSI structural
//     pattern shared by the per-family SystemClock_Config functions.

#ifdef RULOS_USE_HSE

#include <stdbool.h>

#include "stm32.h"

// True if HSE failed at boot, or was lost during operation via the
// Clock Security System NMI. Apps that depend on HSE for timing
// accuracy check this after rulos_hal_init() and refuse to operate
// when set.
extern bool g_rulos_hse_failed;

// Caller-provided function that fills `osc` for the HSI+PLL fallback
// path. Family-specific implementations live in stm32-hardware.c since
// the PLL field types and layout differ across STM32 families.
typedef void (*rulos_hsi_pll_config_fn)(RCC_OscInitTypeDef *osc);

// Try to start the PLL using HSE as input. `osc` must be pre-populated
// with HSE state and PLL multiplier/divider fields. On success, enables
// the Clock Security System so a mid-stream HSE failure fires NMI. On
// failure, sets g_rulos_hse_failed, replaces `osc` contents via
// `hsi_fallback`, and re-runs OscConfig (traps on a second failure).
void rulos_hse_try_pll(RCC_OscInitTypeDef *osc,
                       rulos_hsi_pll_config_fn hsi_fallback);

#endif  // RULOS_USE_HSE
