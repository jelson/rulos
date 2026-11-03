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

// std includes
#include <stdbool.h>
#include <stdint.h>

// ESP32 IDF includes
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_task.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

// RULOS includes
#include "core/hal.h"
#include "core/hardware_types.h"

const int RULOS_STACK_SIZE = 16 * 1024;

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

void rulos_hal_init(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

static void run_rulos_main(void *data) {
  extern int main(void);
  main();
}

void hal_reset() {
  esp_restart();
}

extern "C" {
void app_main(void) {
  gpio_install_isr_service(0);
  xTaskCreatePinnedToCore(run_rulos_main, "rulosMain", RULOS_STACK_SIZE, NULL, 1, NULL,
                          RULOS_ESP32_CORE_ID);
}
}

// For information on how Espressif has modified FreeRTOS to work on SMP, see:
//
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/freertos-smp.html
//
// In particular the section called "Critical Sections" has important
// information on how to enter and exit critical sections in the presence of
// SMP. Merely disabling interrupts does not protect critical sections because
// the other CPU might take an interrupt. portENTER_CRITICAL() both disables
// interrupts (on same-cpu) and then takes a spinlock (to protect against
// interrupts on other-cpu). The _SAFE variants pick the task or ISR form
// based on the calling context; the timer ISR takes this lock too.

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

rulos_irq_state_t hal_start_atomic() {
  portENTER_CRITICAL_SAFE(&mux);
  return 0;
}

void hal_end_atomic(rulos_irq_state_t old_interrupts) {
  portEXIT_CRITICAL_SAFE(&mux);
}

bool hal_is_in_isr(void) {
  return xPortInIsrContext();
}

void hal_idle() {
  // yield to other freeRTOS tasks running on the esp32
  vTaskDelay(1);
}

void hal_idle_atomic() {
  // vTaskDelay cannot run inside a FreeRTOS critical section, so release it
  // around the yield. A wakeup that arrives during the yield is picked up by
  // the caller's re-check after at most one FreeRTOS tick.
  portEXIT_CRITICAL_SAFE(&mux);
  vTaskDelay(1);
  portENTER_CRITICAL_SAFE(&mux);
}

void hal_delay_ms(uint16_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}
