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
#include "core/rulos.h"
#include "curr_meas.h"
#include "flash_dumper.h"
#include "ina219.h"
#include "periph/uart/uart.h"
#include "serial_reader.h"

// uart definitions
#define CONSOLE_UART_NUM 0
#define GPS_UART_NUM     4
#define MODEM_UART_NUM   1
#define PSOC_TX_UART_NUM 3

// power measurement
#define GPS_POWERMEASURE_ADDR   0b1000000
#define MODEM_POWERMEASURE_ADDR 0b1000001

#define LED_PIN GPIO_A8

UartState_t console;
flash_dumper_t flash_dumper;
currmeas_state_t gps_cms, modem_cms;
serial_reader_t psoc_console_tx;
serial_reader_t modem_uart;
serial_reader_t gps_uart;

static void turn_off_led(void *data) {
  gpio_clr(LED_PIN);
}

static void indicate_alive(void *data) {
  LOG("run");
  schedule_us(2000000, indicate_alive, NULL);
  if (flash_dumper.ok && serial_reader_is_active(&psoc_console_tx)) {
    gpio_set(LED_PIN);
    schedule_us(20000, turn_off_led, NULL);
  }
}

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER1);

  // initialize console uart
  uart_init(&console, CONSOLE_UART_NUM, 1000000);
  log_bind_uart(&console);
  LOG("LTE Tag Dev Board Datalogger starting, rev " STRINGIFY(GIT_COMMIT));

  // initialize flash dumper
  flash_dumper_init(&flash_dumper);

  // initialize serial readers
  serial_reader_init(&psoc_console_tx, PSOC_TX_UART_NUM, 1000000, &flash_dumper,
                     NULL, NULL);
#if 0
  serial_reader_init(&modem_uart, MODEM_UART_NUM, 115200, &flash_dumper, NULL);
  serial_reader_init(&gps_uart, GPS_UART_NUM, 115200, &flash_dumper, NULL);
#endif

  // initialize current measurement. docs say calibration register should be
  // trunc[0.04096 / (current_lsb * R_shunt)]

  // GPS: R_shunt is 150 milliohms, Iresolution = 66.7
  // .04096 / (66.7 uA * 0.150 ohms) = 4094
  currmeas_init(&gps_cms, GPS_POWERMEASURE_ADDR, VOLT_PRESCALE_DIV1, 54613, 5,
                GPS_UART_NUM, &flash_dumper);

  // LTE: Rshunt = 0.50 ohms, Iresolution = 10uA
  // .04096 / (10 uA * 0.5 ohms) = 8192
  currmeas_init(&modem_cms, MODEM_POWERMEASURE_ADDR, VOLT_PRESCALE_DIV1, 8192,
                10,  // we read in 10 microamps
                MODEM_UART_NUM, &flash_dumper);

  // enable periodic blink to indicate liveness
  schedule_now(indicate_alive, NULL);

  gpio_make_output(LED_PIN);

  scheduler_run();
}
