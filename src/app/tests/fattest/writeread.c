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
#include "periph/fatfs/ff.h"

//////////////////////////////////////////////////////////////////////////////

// global instead of on the stack so the size is counted
FATFS fatfs;
UartState_t uart;
char fatbuf[4096];
FIL f;

static void try_write(void *data) {
  if (f_mount(&fatfs, "", 0) != FR_OK) {
    LOG("can't mount");
    __builtin_trap();
  }

  LOG("mount successful");

#if SIM
  LOG("trying to format");
  if (f_mkfs("", FM_FAT32, 1024, fatbuf, sizeof(fatbuf)) != FR_OK) {
    LOG("couldn't format");
    return;
  }
#endif

  LOG("\n\nopening file");
  FRESULT retval;
  const char *test_filename = "rwtest2.txt";
  if (f_open(&f, test_filename, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
    LOG("can't open %s", test_filename);
    __builtin_trap();
  }

  LOG("\n\nwriting");
  char testbuf[4096];
  int len = 0;
  for (int i = 0; i < 1; i++) {
    len += sprintf(testbuf + len, "this is test line number %d\n", i++);
  }
  int total_written = 0;
  UINT bytes_written;
  do {
    retval = f_write(&f, testbuf + total_written, len - total_written,
                     &bytes_written);
    if (retval != FR_OK) {
      LOG("write error writing to %s: %d", test_filename, retval);
      return;
    }
    LOG("wrote %u bytes to %s", (uint16_t)bytes_written, test_filename);
    total_written += bytes_written;
  } while (total_written != len);

  LOG("\n\nsyncing");
  // f_sync(&f);
  f_close(&f);

  if (f_open(&f, test_filename, FA_OPEN_EXISTING | FA_READ) != FR_OK) {
    LOG("can't open %s", test_filename);
    __builtin_trap();
  }
  UINT bytes_read;
  int total_read = 0;
  memset(fatbuf, 0, sizeof(fatbuf));
  while (true) {
    retval = f_read(&f, fatbuf + total_read, len - total_read, &bytes_read);
    if (retval != FR_OK) {
      LOG("read error reading from %s: %d", test_filename, retval);
      __builtin_trap();
    }

    // if we've reached EOF, do the comparison
    if (bytes_read == 0) {
      LOG("reached EOF, comparing...");
      f_close(&f);
      if (total_read == total_written) {
        LOG("lengths match!");
      } else {
        LOG("FAILED - lengths do not match");
        return;
      }
      if (!memcmp(fatbuf, testbuf, len)) {
        LOG("success!!!!!!");
      } else {
        LOG("FAILED");
      }
      return;
    }

    total_read += bytes_read;
    LOG("read %u bytes of data from %s, total=%d", (uint16_t)bytes_read,
        test_filename, total_read);
  }
}

int main() {
  hal_init();
  init_clock(10000, TIMER1);

  uart_init(&uart, /* uart_id= */ 0, 1000000);
  log_bind_uart(&uart);
  LOG("Log output running");

  // Give the SD card power 10ms to stabilize
  schedule_us(10000, try_write, NULL);
  cpumon_main_loop();

  return 0;
}
