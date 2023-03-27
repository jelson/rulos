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

#include "flash_dumper.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "core/rulos.h"
#include "core/wallclock.h"

#define FLUSH_PERIOD_MSEC 3000
#define MAX_FNAME_L       (128)

static const char *makeFileName() {
  const char *counter_fname = "filenum.dat";
  static char return_name[MAX_FNAME_L] = {};
  unsigned int count = 0;
  strcpy(return_name, "datalog.txt");

  FIL fp;

  // try to open the counter file
  int open_res = f_open(&fp, counter_fname, FA_READ | FA_WRITE);
  if (open_res == FR_OK) {
    uint32_t bytes_count = 0;
    int file_size = f_size(&fp);
    // if the size of the file is not exactly four bytes,
    // then initialize it with four bytes of zeroes,
    // then jump back to beginning
    if (file_size != 4) {
      LOG("Looks like a new counter file.");
      f_write(&fp, &count, 4, &bytes_count);
      f_sync(&fp);
      f_lseek(&fp, 0);
      if (bytes_count == 4) {
        LOG("counter initialized");
      }
    }
    int rd_ok = f_read(&fp, &count, 4, &bytes_count);
    if ((rd_ok == FR_OK) && (bytes_count == 4)) {
      // read four bytes and if successful, use them as the
      // counter value
      LOG("Success Reading %s. File count is %u", counter_fname, count);
      snprintf(return_name, MAX_FNAME_L - 1, "log_%03u.txt", count % 1000);
      count += 1;
      f_lseek(&fp, 0);
      bytes_count = 0;
      // finally, increment the count and write it back to the file
      int wr_ok = f_write(&fp, &count, 4, &bytes_count);
      if ((wr_ok == FR_OK) && (bytes_count == 4)) {
        LOG("File count updated.");
      }
      f_sync(&fp);
      f_close(&fp);
    }
    f_close(&fp);
  } else if ((open_res == FR_NO_FILE) || (open_res == FR_NO_PATH)) {
    // If there is no counter file, that's ok, we'll just use
    // the fixed name
    LOG("No %s found", counter_fname);
  } else {
    // there is a counter file but it could not be opened for some
    // other reasons...
    LOG("Failed to open %s; Error: %d.", counter_fname, open_res);
  }

  LOG("makeFileName returning: %s", return_name);

  return return_name;
}

static void flash_dumper_periodic_flush(void *data) {
  flash_dumper_t *fd = (flash_dumper_t *)data;
  schedule_us(FLUSH_PERIOD_MSEC * 1000, flash_dumper_periodic_flush, fd);

  // do nothing if we're already in the error state
  if (!fd->ok) {
    LOG("flash dumper: flash in error state");
    return;
  }

  // try to sync
  int retval = f_sync(&fd->fp);
  if (retval != FR_OK) {
    fd->ok = false;
    LOG("flash dumper: error %d!!", retval);
    return;
  }

  LOG("flash dumper: log size %ld bytes", fd->bytes_written);
}

void flash_dumper_init(flash_dumper_t *fd) {
  memset(fd, 0, sizeof(*fd));

  wallclock_init(&fd->wallclock);

  // create periodic task for flushing cache
  schedule_now(flash_dumper_periodic_flush, fd);

  if (f_mount(&fd->fatfs, "", 0) == FR_OK) {
    LOG("FatFS mounted");
  } else {
    LOG("Failure mounting FatFS");
    return;
  }

  const char *filename = makeFileName();

  LOG("trying to create file");
  int retval = f_open(&fd->fp, filename, FA_WRITE | FA_OPEN_APPEND);
  if (retval != FR_OK) {
    LOG("can't open file: %d", retval);
    return;
  }
  LOG("opened file ok");
  fd->ok = true;

  flash_dumper_print(fd, "startup," STRINGIFY(GIT_COMMIT));
}

static bool _write_to_file(flash_dumper_t *fd, const void *buf, uint32_t len) {
  // attempt to write to the SD card
  uint32_t written;
  int retval = f_write(&fd->fp, buf, len, &written);
  if (retval != FR_OK) {
    LOG("couldn't write to sd card: got retval of %d", retval);
    fd->ok = false;
    return false;
  }
  if (written == 0) {
    LOG("error writing to SD card: nothing written");
    fd->ok = false;
    return false;
  }
  if (written != len) {
    LOG("tried to write %ld, but wrote %ld!?", len, written);
    return false;
  }

  fd->bytes_written += len;

  return true;
}

// making this static instead of a local variable so it's accounted for in our
// total memory use
static char prefix_buf[50];

void flash_dumper_write(flash_dumper_t *fd, const void *buf, uint32_t len,
                        const char *prefix_fmt, ...) {
  // prepend all lines with the current time in milliseconds and a comma
  uint32_t sec, usec;
  wallclock_get_uptime(&fd->wallclock, &sec, &usec);
  int prefix_len = sprintf(prefix_buf, "\n%ld,", sec * 1000 + usec / 1000);

  // append the prefix passed in by the caller
  if (prefix_fmt != NULL) {
    va_list ap;
    va_start(ap, prefix_fmt);
    prefix_len += vsnprintf(prefix_buf + prefix_len,
                            sizeof(prefix_buf) - prefix_len, prefix_fmt, ap);
  }

  // write the header to the file
  if (!_write_to_file(fd, prefix_buf, prefix_len)) {
    return;
  }

  // write the data to the file
  if (buf != NULL && len > 0) {
    if (!_write_to_file(fd, buf, len)) {
      return;
    }
  }

#if DUMP_TO_CONSOLE
  log_write(prefix_buf, prefix_len);
  log_write(buf, r_min(len, 30));
#endif
}

void flash_dumper_print(flash_dumper_t *fd, const char *s) {
  flash_dumper_write(fd, s, strlen(s), NULL);
}
