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
HalUart uart;
char fatbuf[4096];
FIL f;

static void read_4meg_file(r_bool validate) {
  if (f_open(&f, "4meg.bin", FA_OPEN_EXISTING | FA_READ) != FR_OK) {
    LOG("can't open 1meg");
    __builtin_trap();
  }

  long total = 0;
  uint32_t expected_next_num = 0;
  const uint32_t expected_increment = 4093;
  uint32_t num_validated = 0;
  UINT bytes_read;
  FRESULT retval;
  
  volatile Time start = precise_clock_time_us();
  for (int i = 0; i < 1024; i++) {
    retval = f_read(&f, fatbuf, sizeof(fatbuf), &bytes_read);
    if (retval != FR_OK) {
      LOG("read error %d", retval);
      __builtin_trap();
    }
    if (bytes_read < sizeof(fatbuf)) {
      LOG("unexpected EOF, read %lu bytes", bytes_read);
      __builtin_trap();
    }
    total += bytes_read;

    if (validate) {
      uint32_t curr_num_bigendian;
      uint8_t *ptr = (uint8_t *) &curr_num_bigendian;
      uint32_t *fatbuf_uint32 = (uint32_t *) fatbuf;

      for (int j = 0; j < sizeof(fatbuf) / 4; j++) {
	ptr[0] = expected_next_num >> 24;
	ptr[1] = expected_next_num >> 16;
	ptr[2] = expected_next_num >> 8;
	ptr[3] = expected_next_num;

	if (*fatbuf_uint32 != curr_num_bigendian) {
	  LOG("validation error: ");
	  LOG("  validation index: %lu", num_validated);
	  LOG("  position within buffer: %d", j);
	  LOG("  expected number: %lu", expected_next_num);
	  LOG("  expected big-endian: %02x %02x %02x %02x", ptr[0], ptr[1], ptr[2], ptr[3]);
	  LOG("  actual number: %lu", *fatbuf_uint32);
	  __builtin_trap();
	}
	num_validated++;
	fatbuf_uint32++;
	expected_next_num += expected_increment;
      }
    }
  }
  volatile Time end = precise_clock_time_us();
  volatile Time elapsed = end - start;

  // Validate that we reached the end of the file
  retval = f_read(&f, fatbuf, sizeof(fatbuf), &bytes_read);

  if (retval != FR_OK) {
    LOG("read error on final read: %d", retval);
    __builtin_trap();
  }
  if (bytes_read != 0) {
    LOG("expected an EOF on final read, but instead got %lu bytes", bytes_read);
    __builtin_trap();
  }
  
  f_close(&f);

  if (validate) {
    LOG("read and validated %lu kbyte in %ld msec", total / 1024, elapsed / 1000);
  } else {
    LOG("read benchmark: %lu kbyte in %ld msec", total / 1024, elapsed / 1000);
  }
}

static void try_read(void* data) {
  if (f_mount(&fatfs, "", 0) != FR_OK) {
    LOG("can't mount");
    __builtin_trap();
  }

  LOG("mount successful");

  UINT bytes_read;
  FRESULT retval;
  const char test_filename[] = "rocket2.txt";
  
  if (f_open(&f, test_filename, FA_OPEN_EXISTING | FA_READ) != FR_OK) {
    LOG("can't open %s", test_filename);
    __builtin_trap();
  }
  retval = f_read(&f, fatbuf, sizeof(fatbuf), &bytes_read);
  if (retval != FR_OK) {
    LOG("read error reading from %s: %d", test_filename, retval);
    __builtin_trap();
  }
  fatbuf[bytes_read] = '\0';
  LOG("read data from %s: %s", test_filename, fatbuf);
  f_close(&f);
  
  read_4meg_file(true);
  read_4meg_file(false);

  __builtin_trap();
}

int main() {
  hal_init();
  init_clock(10000, TIMER1);

  hal_uart_init(&uart, 115200, true, /* uart_id= */ 0);
  LOG("Log output running");

  // Give the SD card power 10ms to stabilize
  schedule_us(10000, try_read, NULL);
  cpumon_main_loop();

  return 0;
}
