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

#include "core/hardware.h"
#include "core/hal.h"
#include "core/logging.h"

#undef FALSE
#undef TRUE

#include "chip.h"

// This is the first entry point defined by the linker script:
// meant for early setup such as clock configuration. This is done before, for
// example, bss is copied into RAM.
void SystemInit() {
  // LPC library function that configures the internal RC oscillator as the
  // system clock
  Chip_SystemInit();
}

// This is the second entry point defined by the linker script:
// after initialization is complete. We call the program's main().
void _start() {
  extern int main(void);
  main();
}

// RULOS-specific init function. We use this to initialize various modules on
// the chip.
void hal_init() {
  // init GPIO subsystem
  Chip_GPIO_Init(LPC_GPIO);

  // Eventually, more init will go here as we use more peripherals
}

rulos_irq_state_t hal_start_atomic() {
  rulos_irq_state_t old_interrupts = __get_PRIMASK();
  __disable_irq();
  return old_interrupts;
}

void hal_end_atomic(rulos_irq_state_t old_interrupts) {
  __set_PRIMASK(old_interrupts);
}

void arm_assert(const uint32_t line) {
  LOG("assertion failed: line %d", line);
  LOG_FLUSH();
  __builtin_trap();
}
