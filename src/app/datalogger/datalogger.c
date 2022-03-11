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
#define DUT1_UART_NUM    1
#define DUT2_UART_NUM    3
#define REFGPS_UART_NUM  4

// pin definitions
#define REC_LED_PIN  GPIO_B2
#define DUT1_LED_PIN GPIO_A12
#define DUT2_LED_PIN GPIO_A11
#define DUT1_PWR_PIN GPIO_B4
#define DUT2_PWR_PIN GPIO_B5

// power measurement
#define DUT1_POWERMEASURE_ADDR 0b1000001
#define DUT2_POWERMEASURE_ADDR 0b1000000

UartState_t console;
flash_dumper_t flash_dumper;
serial_reader_t refgps, dut1, dut2;
currmeas_state_t cms1, cms2;

//// sony config

static const char *sony_init_seq[][2] = {
    {"VER", "[VER] Done"},
    {"BSSL 34345967", "[BSSL] Done"},
    {"GNS 0x3", "[GNS] Done"},
    {"GSOP 1 1000 0", "[GSOP] Done"},
    {"GSR", "[GSR] Done"},
    {"GCD", "[GCD] Done"},
    {NULL, NULL},
};

static int last_sony_step = -1;

static void write_sony_config_string(serial_reader_t *sr) {
  const char *next = sony_init_seq[last_sony_step][0];
  if (next == NULL) {
    LOG("sony config done");
    last_sony_step = -1;
    return;
  }

  char s[100];
  LOG("sending sony config step %d", last_sony_step);
  sprintf(s, "@%s\r\n", next);
  serial_reader_print(sr, s);
}

static void sony_config_received(serial_reader_t *sr, const char *line) {
  const char *expected = sony_init_seq[last_sony_step][1];
  if (last_sony_step >= 0 && strlen(line) >= strlen(expected) &&
      !strncmp(line, expected, strlen(expected))) {
    last_sony_step++;
    write_sony_config_string(sr);
  }
}

static void enable_sony(void *data) {
  serial_reader_t *sr = (serial_reader_t *)data;
  if (!serial_reader_is_active(sr)) {
    LOG("starting sony init");
    gpio_set(DUT1_PWR_PIN);
    last_sony_step = 0;
    write_sony_config_string(sr);
  } else {
    Time now = clock_time_us();
    LOG("sony seems okay (now %ld, last %ld, %ld ago", now, sr->last_active,
        now - sr->last_active);
  }
  schedule_us(5000000, enable_sony, data);
}

//// ublox config

static uint8_t ublox_config[] = {
    // Ublox config: UBX-CFG-GNSS, to enable GPS, Galileo, GLONASS
    0xB5, 0x62, 0x06, 0x3E, 0x34, 0x00, 0x00, 0x00, 0x3F, 0x06, 0x00, 0x08,
    0x10, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x03, 0x03, 0x00, 0x01, 0x00,
    0x01, 0x01, 0x02, 0x08, 0x0C, 0x00, 0x01, 0x00, 0x01, 0x01, 0x03, 0x02,
    0x05, 0x00, 0x00, 0x00, 0x01, 0x01, 0x05, 0x03, 0x04, 0x00, 0x01, 0x00,
    0x05, 0x01, 0x06, 0x08, 0x0C, 0x00, 0x01, 0x00, 0x01, 0x01, 0x37, 0x04};

bool ublox_glonass_active = false;

static void enable_ublox(void *data) {
  serial_reader_t *sr = (serial_reader_t *)data;

  gpio_set(DUT2_PWR_PIN);

  if (!ublox_glonass_active) {
    LOG("sending ublox gnss config");
    uart_write(&sr->uart, ublox_config, sizeof(ublox_config));

    char flashlog[300];
    int len = snprintf(flashlog, sizeof(flashlog),
                       "out,%d,<ublox config string>", sr->uart.uart_id);
    flash_dumper_write(sr->flash_dumper, flashlog, len);
  }
  ublox_glonass_active = false;

  schedule_us(5000000, enable_ublox, data);
}

static void ublox_config_received(serial_reader_t *sr, const char *line) {
  if (!strncmp(line, "$GLGSV", strlen("$GLGSV"))) {
    ublox_glonass_active = true;
  }
}

static void indicate_alive(void *data) {
  static bool onoff = false;
  LOG("run");
  schedule_us(250000, indicate_alive, NULL);
  if (!flash_dumper.ok || !serial_reader_is_active(&refgps)) {
    gpio_clr(REC_LED_PIN);
  } else {
    gpio_set_or_clr(REC_LED_PIN, onoff);
    onoff = !onoff;
  }

  gpio_set_or_clr(DUT1_LED_PIN, serial_reader_is_active(&dut1));
  gpio_set_or_clr(DUT2_LED_PIN, serial_reader_is_active(&dut2));
}

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER1);

  // initialize console uart
  uart_init(&console, CONSOLE_UART_NUM, 1000000);
  log_bind_uart(&console);
  LOG("Datalogger starting");

  // initialize pins
  gpio_make_output(REC_LED_PIN);
  gpio_make_output(DUT1_LED_PIN);
  gpio_make_output(DUT2_LED_PIN);
  gpio_make_output(DUT1_PWR_PIN);
  gpio_make_output(DUT2_PWR_PIN);

  // turn off power to DUT1 and DUT2
  gpio_clr(DUT1_PWR_PIN);
  gpio_clr(DUT2_PWR_PIN);

  // initialize flash dumper
  flash_dumper_init(&flash_dumper);

  // initialize reference gps
  serial_reader_init(&refgps, REFGPS_UART_NUM, 9600, &flash_dumper, NULL);

  // enable sony on dut1
  serial_reader_init(&dut1, DUT1_UART_NUM, 115200, &flash_dumper,
                     sony_config_received);
  schedule_us(1000000, enable_sony, &dut1);

  // enable ublox on dut2
  serial_reader_init(&dut2, DUT2_UART_NUM, 38400, &flash_dumper,
                     ublox_config_received);
  schedule_us(1000000, enable_ublox, &dut2);

  // initialize current measurement

  // docs say calibration register should be trunc[0.04096 / (current_lsb *
  // R_shunt)] R_shunt for the sony is 5.1 ohms, we'll set current_lsb to be 1
  // microamp; that gives us 8031. manually calibrated using 34401A to get a
  // better value.
  currmeas_init(&cms1, DUT1_POWERMEASURE_ADDR, VOLT_PRESCALE_DIV1, 8577,
                1,  // we read in microamps
                DUT1_UART_NUM, &flash_dumper);

  // the ublox has a builtin current shunt of 0.1 ohms. we must make current_lsb
  // 0.01mA so we get 40960 as the prescale
  currmeas_init(&cms2, DUT2_POWERMEASURE_ADDR, VOLT_PRESCALE_DIV1, 40960,
                10,  // we read in 10 microamps
                DUT2_UART_NUM, &flash_dumper);

  // enable periodic blink to indicate liveness
  schedule_now(indicate_alive, NULL);

  scheduler_run();
}
