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

#include "core/rulos.h"
#include "hardware.h"
#include "periph/bss_canary/bss_canary.h"

#define FREQ_USEC 50000

#if defined(RULOS_ARM)
#define TEST_PIN GPIO3_03
#elif defined(RULOS_AVR)
#define TEST_PIN GPIO_B3
#else
#error "No test pin defined"
#endif

void test_func(void *data) {
  schedule_us(FREQ_USEC, (ActivationFuncPtr)test_func, NULL);
}

int main() {
  hal_init();

  bss_canary_init();

  gpio_make_output(TEST_PIN);
  while (1) {
    gpio_set(TEST_PIN);
    gpio_clr(TEST_PIN);
  }

  init_clock(FREQ_USEC, TIMER1);

  schedule_now((ActivationFuncPtr)test_func, NULL);

  cpumon_main_loop();
}
