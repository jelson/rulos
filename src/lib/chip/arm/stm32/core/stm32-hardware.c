/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#include "core/arm-hal.h"
#include "core/hal.h"
#include "core/hardware.h"

void _init() {}

void hal_init() {
  // Initialize the STM32 HAL library. (Good thing the name didn't
  // collide...)
  HAL_Init();
}

uint32_t arm_hal_get_clock_rate() { return HAL_RCC_GetSysClockFreq(); }

static void* g_timer_data = NULL;
static Handler g_timer_handler = NULL;

void SysTick_Handler() {
  if (g_timer_handler != NULL) {
    g_timer_handler(g_timer_data);
  }
}

void arm_hal_start_clock_us(uint32_t ticks_per_interrupt, Handler handler,
                            void* data) {
  g_timer_handler = handler;
  g_timer_data = data;

  // Setup the timer to fire interrupts at the requested interval.
  SysTick_Config(ticks_per_interrupt);
}
