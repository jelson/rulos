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
#include "periph/fatfs/ff.h"
#include "periph/uart/linereader.h"
#include "periph/uart/uart.h"

// uart definitions
#define CONSOLE_UART_NUM 0
#define DUT1_UART_NUM    1
#define DUT2_UART_NUM    2
#define REFGPS_UART_NUM  4

// pin definitions
#define REC_LED_PIN      GPIO_B2
#define DUT1_LED_PIN     GPIO_A12
#define DUT2_LED_PIN     GPIO_A11
#define DUT1_PWR_PIN     GPIO_B4
#define DUT2_PWR_PIN     GPIO_B5

// minimum length of data to buffer before writing to FAT
#define WRITE_INCREMENT 2048

typedef struct {
  FATFS fatfs;  // SD card filesystem global state
  FIL fp;
  char buf[WRITE_INCREMENT * 2];
  int len;
  bool ok;
} flash_dumper_t;

typedef struct {
  UartState_t uart;
  LineReader_t linereader;
  flash_dumper_t *flash_dumper;
  int num_total_lines;
  //gpio_pin_t led;
} serial_reader_t;

UartState_t console;
flash_dumper_t flash_dumper;
serial_reader_t refgps, dut1, dut2;

void flash_dumper_init(flash_dumper_t *fd) {
  const char *filename = "log.txt";
  gpio_clr(REC_LED_PIN);

  memset(fd, 0, sizeof(*fd));
  if (f_mount(&fd->fatfs, "", 0) != FR_OK) {
    LOG("can't mount flash filesystem");
    return;
  }

  LOG("trying to create file");
  int retval = f_open(&fd->fp, filename, FA_WRITE | FA_OPEN_APPEND);
  if (retval != FR_OK) {
    LOG("can't open file: %d", retval);
    return;
  }
  LOG("opened file ok");
  gpio_set(REC_LED_PIN);
}

void flash_dumper_append(flash_dumper_t *fd, const void *buf, int len) {
  // make sure there's sufficient space
  if (len + fd->len > sizeof(fd->buf)) {
    LOG("ERROR: Flash buf overflow!");
  } else {
    // append buffer
    memcpy(&fd->buf[fd->len], buf, len);
    fd->len += len;
  }

  // if we've reached 2 complete FAT sectors, write
  while (fd->len >= WRITE_INCREMENT) {
    // clear the LED so it stays out if there's a write error
    gpio_clr(REC_LED_PIN);

    uint32_t written;
    int retval = f_write(&fd->fp, fd->buf, WRITE_INCREMENT, &written);
    if (retval != FR_OK) {
      LOG("couldn't write to sd card: got retval of %d", retval);
      return;
    }
    if (written == 0) {
      LOG("error writing to SD card: nothing written");
      return;
    }
    retval = f_sync(&fd->fp);
    if (retval != FR_OK) {
      LOG("couldn't sync: retval %d", retval);
      return;
    }

    // success!
    gpio_set(REC_LED_PIN);
    LOG("flash: wrote %ld bytes", written);
    memmove(&fd->buf[0], &fd->buf[written], fd->len - written);
    fd->len -= written;
  }
}

static void sr_line_received(UartState_t *uart, void *user_data, char *line) {
  serial_reader_t *sr = (serial_reader_t *)user_data;
  char buf[128];
  int len = snprintf(buf, sizeof(buf), "u,%d,%d,%s\n", uart->uart_id,
                     sr->num_total_lines++, line);
  uart_write(&console, buf, len);
  flash_dumper_append(sr->flash_dumper, buf, len);
}

static void serial_reader_init(serial_reader_t *sr, uint8_t uart_id,
                               uint32_t baud, flash_dumper_t *flash_dumper) {
  memset(sr, 0, sizeof(*sr));
  sr->flash_dumper = flash_dumper;
  uart_init(&sr->uart, uart_id, baud, true);
  linereader_init(&sr->linereader, &sr->uart, sr_line_received, sr);
}

static void enable_sony(void *data) {
  serial_reader_t *sr = (serial_reader_t *)data;
  uart_print(&sr->uart, "\r\n@GCD\r\n");
  LOG("run");
  schedule_us(250000, enable_sony, data);
}

int main() {
  hal_init();
  init_clock(10000, TIMER1);

  // initialize console uart
  uart_init(&console, CONSOLE_UART_NUM, 1000000, true);
  log_bind_uart(&console);
  LOG("Datalogger starting");

  // initialize pins
  gpio_make_output(REC_LED_PIN);
  gpio_make_output(DUT1_LED_PIN);
  gpio_make_output(DUT2_LED_PIN);
  gpio_make_output(DUT1_PWR_PIN);
  gpio_make_output(DUT2_PWR_PIN);

  // turn on power to DUT1
  gpio_set(DUT1_PWR_PIN);

  // initialize flash dumper
  flash_dumper_init(&flash_dumper);

  // initialize reference gps
  serial_reader_init(&refgps, REFGPS_UART_NUM, 9600, &flash_dumper);

  // enable sony on dut1
  serial_reader_init(&dut1, DUT1_UART_NUM, 115200, &flash_dumper);
  schedule_now(enable_sony, &dut1);

  cpumon_main_loop();
}
