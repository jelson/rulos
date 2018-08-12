#include "core/hal.h"

#undef FALSE
#undef TRUE

#include "chip/arm/lpc_chip_11cxx_lib/chip.h"
#include "chip/arm/lpc_chip_11cxx_lib/gpio_11xx_2.h"

void SystemInit()
{
  Chip_SystemInit();
}

void hal_init()
{
  Chip_GPIO_Init(LPC_GPIO);
}

rulos_irq_state_t hal_start_atomic()
{
  rulos_irq_state_t old_interrupts = __get_PRIMASK();
  __disable_irq();
  return old_interrupts;
}

void hal_end_atomic(rulos_irq_state_t old_interrupts)
{
  __set_PRIMASK(old_interrupts);
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
