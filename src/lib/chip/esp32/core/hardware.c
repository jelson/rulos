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
#include "esp_attr.h"
#include "esp_task.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "soc/rtc.h"
#include "xtensa/xtruntime.h"

const int RULOS_STACK_SIZE = 16 * 1024;
const int __attribute__((used)) DRAM_ATTR uxTopUsedPriority =
    configMAX_PRIORITIES - 1;

// when we run RULOS on core 0, we get watchdog timer timeouts unless
// we put vTaskDelay() in hal_idle() -- I guess there are other
// FreeRTOS tasks pinned to core 0, and they time out unless RULOS
// yields to them.
//
// Running RULOS on core 1, we do not get watchdog timeouts even
// without vTaskDelay().
//
// Belt-and-suspenders solution: run on core 1, *and* put a call to
// vTaskDelay() in hal_idle().
const int RULOS_ESP32_CORE_ID = 1;

void rulos_hal_init(void) {
  // TODO move this to esp32 hal init
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

static void run_rulos_main(void* data) {
  extern int main(void);
  main();
}

extern "C" {
void app_main(void) {
  xTaskCreatePinnedToCore(run_rulos_main, "rulosMain", RULOS_STACK_SIZE, NULL,
                          1, NULL, RULOS_ESP32_CORE_ID);
}
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
  // yield to other freeRTOS tasks running on the esp32
  vTaskDelay(1);
}
