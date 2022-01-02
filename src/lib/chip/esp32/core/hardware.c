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

#include "core/hardware.h"

#include <stdbool.h>
#include <stdint.h>

#include "core/hal.h"
#include "core/hardware_types.h"
#include "soc/rtc.h"
#include "xtensa/xtruntime.h"
#include "esp_attr.h"
#include "esp_task.h"

const int __attribute__((used)) DRAM_ATTR uxTopUsedPriority = configMAX_PRIORITIES - 1;

void hal_init(void) {
}

void app_main(void) {
  extern int main(void);
  main();
}

static uint32_t calculateApb(rtc_cpu_freq_config_t* conf) {
  if (conf->freq_mhz >= 80) {
    return 80000000;
  }
  return (conf->source_freq_mhz * 1000000) / conf->div;
}

uint32_t getApbFrequency() {
  rtc_cpu_freq_config_t conf;
  rtc_clk_cpu_freq_get_config(&conf);
  return calculateApb(&conf);
}

rulos_irq_state_t hal_start_atomic() {
  return XTOS_SET_INTLEVEL(XCHAL_EXCM_LEVEL);
}

void hal_end_atomic(rulos_irq_state_t old_interrupts) {
  XTOS_RESTORE_JUST_INTLEVEL(old_interrupts);
}

void hal_idle() {
}

#if 0

uint32_t hal_start_clock_us(uint32_t us, Handler handler, void *data, uint8_t timer_id) {return 0;} 
uint16_t hal_elapsed_milliintervals() { return 0; }
bool hal_clock_interrupt_is_pending() { return false; }

#endif
