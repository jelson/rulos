#include "hal.h"

void SystemInit()
{
}

void hal_init()
{
}

uint8_t hal_start_atomic()
{
  return 0;
}

void hal_end_atomic(uint8_t old_interrupts)
{
}

uint32_t hal_start_clock_us(uint32_t us, Handler handler, void *data, uint8_t timer_id)
{
  return 0;
}

void hal_idle()
{
  //  __WFI();
}


void hardware_assert(uint16_t line)
{
}
