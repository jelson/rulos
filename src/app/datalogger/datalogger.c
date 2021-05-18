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

#define WRITE_INCREMENT 512

typedef struct {
  FATFS fatfs;  // SD card filesystem global state
  FIL fp;
  char buf[8192];
  int len;
  bool ok;
} flash_dumper_t;

typedef struct {
  UartState_t uart;
  LineReader_t linereader;
  flash_dumper_t *flash_dumper;
  int num_total_lines;
  char buf[4096];
  int len;
} serial_reader_t;

UartState_t console;
flash_dumper_t flash_dumper;
serial_reader_t refgps;

void flash_dumper_init(flash_dumper_t *fd) {
  const char *filename = "log.txt";

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
}

void flash_dumper_append(flash_dumper_t *fd, const void *buf, int len) {
  // make sure there's sufficient space
  if (len + fd->len > sizeof(fd->buf)) {
    LOG("Flash buf overflow!");
    return;
  }

  // append buffer
  memcpy(&fd->buf[fd->len], buf, len);
  fd->len += len;

  // if we've reached a complete FAT sector, write
  while (fd->len >= WRITE_INCREMENT) {
    uint32_t written;
    int retval = f_write(&fd->fp, fd->buf, fd->len, &written);
    if (retval != FR_OK) {
      LOG("couldn't write to sd card: got retval of %d", retval);
      return;
    }
    if (written == 0) {
      LOG("error writing to SD card: nothing written");
      return;
    }
    f_sync(&fd->fp);
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
  if (sr->len + len < sizeof(sr->buf)) {
    memcpy(&sr->buf[sr->len], buf, len);
    sr->len += len;
  } else {
    LOG("UART %d BUFFER OVERFLOW", uart->uart_id);
  }
}

static void sr_dump(void *user_data) {
  serial_reader_t *sr = (serial_reader_t *)user_data;
  schedule_us(1000000 + (sr->uart.uart_id * 10000), sr_dump, sr);
  if (sr->len > 0) {
    LOG("u%d: queueing %d bytes", sr->uart.uart_id, sr->len);
    flash_dumper_append(sr->flash_dumper, sr->buf, sr->len);
    sr->len = 0;
  }
}

static void serial_reader_init(serial_reader_t *sr, uint8_t uart_id,
                               uint32_t baud, flash_dumper_t *flash_dumper) {
  memset(sr, 0, sizeof(*sr));
  sr->flash_dumper = flash_dumper;
  uart_init(&sr->uart, uart_id, baud, true);
  linereader_init(&sr->linereader, &sr->uart, sr_line_received, sr);
  schedule_us(1, sr_dump, sr);
}

int main() {
  hal_init();
  init_clock(10000, TIMER1);

  // initialize console uart
  uart_init(&console, /* uart_id= */ 0, 1000000, true);
  log_bind_uart(&console);
  LOG("Datalogger starting");

  // initialize flash dumper
  flash_dumper_init(&flash_dumper);

  // initialize reference gps reader
  serial_reader_init(&refgps, 4, 9600, &flash_dumper);

  cpumon_main_loop();
}
