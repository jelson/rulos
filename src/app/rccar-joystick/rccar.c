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

typedef struct {
  bool state;
} Blink;

void blinkfunc(Blink* blink) {
  gpio_set_or_clr(GPIO_C5, blink->state & 1);
  blink->state = 1 - blink->state;
  schedule_us(100000, (ActivationFuncPtr)blinkfunc, &blink);
}

/************************************************************************************/
/************************************************************************************/

int main() {
  hal_init();
  init_clock(10000, TIMER1);
  gpio_make_output(GPIO_C5);

  CpumonAct cpumon;
  cpumon_init(&cpumon);  // includes slow calibration phase

  Blink blink;
  blink.state = 1;
  schedule_us(100000, (ActivationFuncPtr)blinkfunc, &blink);

  // install_handler(ADC, adc_handler);

  cpumon_main_loop();

  return 0;
}
