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

#include "core/rulos.h"
#include "periph/fatfs/diskio.h"

//////////////////////////////////////////////////////////////////////////////

UartState_t uart;

const int START_SECTOR = 4000;
const int SECTOR_SIZE = 512;
const int NUM_SECTORS = 2500;
const int MULTI = 20;
const int SEED = 99;

int sector_errors = 0;

int special_test_sectors[] = {
    40961, 1, 40960, 41040, 11430, 8193,
};

void write_sectors(int sector, int count) {
  BYTE outbuf[SECTOR_SIZE * MULTI];
  BYTE pseudorand = sector + SEED;
  for (int i = 0; i < SECTOR_SIZE * count; i++) {
    outbuf[i] = pseudorand;
    pseudorand = pseudorand * 13 + 7;
  }
  DRESULT retval = disk_write(0, outbuf, sector, count);
  if (retval != RES_OK) {
    sector_errors++;
    LOG("ERROR %d writing sector %d", retval, sector);
    LOG_FLUSH();
    __builtin_trap();
  } else {
    // LOG("success writing sector %d", sector);
  }
}

void read_and_verify_sectors(int sector, int count) {
  BYTE inbuf[SECTOR_SIZE * MULTI];
  // LOG("reading sector %d", sector);
  DRESULT retval = disk_read(0, inbuf, sector, count);
  if (retval != RES_OK) {
    sector_errors++;
    LOG("ERROR %d reading sector %d", retval, sector);
    return;
  }

  int errors = 0;
  BYTE pseudorand = sector + SEED;
  for (int i = 0; i < SECTOR_SIZE * count; i++) {
    if (inbuf[i] != pseudorand) {
      sector_errors++;
      LOG("Data mismatch! Sector %d, byte %d. Exp %d, got %d", sector, i,
          pseudorand, inbuf[i]);
      if (++errors > 10) {
        LOG("aborting sector, too many errors");
        return;
      }
    }
    pseudorand = pseudorand * 13 + 7;
  }
  // LOG("sector %d verified OK", sector);
}

void do_test(void *data) {
  DRESULT retval = disk_initialize(0);
  if (retval != RES_OK) {
    LOG("can't initialize disk - retval %d", retval);
    return;
  }

#if 1
  volatile Time write_start = precise_clock_time_us();
  for (int sector = START_SECTOR; sector < START_SECTOR + NUM_SECTORS;
       sector += MULTI) {
    write_sectors(sector, MULTI);
  }
  volatile Time read_start = precise_clock_time_us();
  for (int sector = START_SECTOR; sector < START_SECTOR + NUM_SECTORS;
       sector += MULTI) {
    read_and_verify_sectors(sector, MULTI);
  }
  volatile Time read_end = precise_clock_time_us();
#endif

  int num_special =
      sizeof(special_test_sectors) / sizeof(special_test_sectors[0]);
  for (int i = 0; i < num_special; i++) {
    write_sectors(special_test_sectors[i], 1);
  }
  for (int i = 0; i < num_special; i++) {
    read_and_verify_sectors(special_test_sectors[num_special - i - 1], 1);
  }

  LOG("done -- %d total sector errors", sector_errors);

  int speed_test_bytes = NUM_SECTORS * SECTOR_SIZE;
  int write_time = read_start - write_start;
  int write_speed = speed_test_bytes / (write_time / 1000);
  LOG("write speed: %d bytes in %d usec, %d kB/sec", speed_test_bytes,
      write_time, write_speed);

  int read_time = read_end - read_start;
  int read_speed = speed_test_bytes / (read_time / 1000);
  LOG("read speed: %d bytes in %d usec, %d kB/sec", speed_test_bytes, read_time,
      read_speed);
}

int main() {
  hal_init();
  init_clock(10000, TIMER1);

  uart_init(&uart, /* uart_id= */ 0, 1000000, true);
  log_bind_uart(&uart);
  LOG("Log output running");

  schedule_us(20000, do_test, NULL);
  cpumon_main_loop();
  return 0;
}
