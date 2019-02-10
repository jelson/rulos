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
#include "core/rulos.h"
#include "periph/bss_canary/bss_canary.h"

#define FREQ_USEC 50000

#if defined(RULOS_ARM_LPC)
#define TEST_PIN GPIO0_08
#elif defined(RULOS_ARM_STM32)
#define TEST_PIN GPIO_A8
#elif defined(RULOS_AVR)
#define TEST_PIN GPIO_B3
#else
#error "No test pin defined"
#endif

#ifdef LOG_TO_SERIAL
HalUart uart;
#endif
void test_func(void *data) {
  gpio_set(TEST_PIN);
#ifdef LOG_TO_SERIAL
  hal_uart_sync_send(
      &uart,
      "hello there this is an extremely long message, one that actually "
      "exceeds the send buffer size of 128 bytes. why would you want to send a "
      "message this long? who knows. i don't judge. i just transmit.\n\n");
  //  LOG("serial output %d emitted", i++);
#endif
  gpio_clr(TEST_PIN);

  schedule_us(FREQ_USEC, (ActivationFuncPtr)test_func, NULL);
}

int main() {
  hal_init();

#ifdef LOG_TO_SERIAL
  hal_uart_init(&uart, 115200, true, /* uart_id= */ 0);
  LOG("Log output running");
#endif

  init_clock(10000, TIMER1);
  gpio_make_output(TEST_PIN);

  bss_canary_init();

  schedule_now((ActivationFuncPtr)test_func, NULL);

  cpumon_main_loop();
}
