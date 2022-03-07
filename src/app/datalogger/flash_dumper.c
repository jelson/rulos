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

#include <stdio.h>
#include <string.h>

#include "core/rulos.h"
#include "core/wallclock.h"

#define DUMP_PERIOD_MSEC 2500

static void flash_dumper_periodic_flush(void *data);

#define MAX_FNAME_L (128)

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
	if ((rd_ok == FR_OK) && (bytes_count== 4)) {
	    // read four bytes and if successful, use them as the
	    // counter value
            LOG("Success Reading %s. File count is %u", counter_fname, count);
            snprintf(return_name, MAX_FNAME_L-1, "log_%03u.txt", count % 1000);
	    count += 1;
	    f_lseek(&fp, 0);
            bytes_count = 0;
	    // finally, increment the count and write it back to the 
	    // file
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
}

// returns true on success, false on failure
static bool flash_dumper_flush(flash_dumper_t *fd, uint32_t flush_size) {
  if (!fd->ok) {
    return false;
  }

  assert(fd->len >= flush_size);
  uint32_t written;
  int retval = f_write(&fd->fp, fd->buf, flush_size, &written);
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
  retval = f_sync(&fd->fp);
  if (retval != FR_OK) {
    fd->ok = false;
    return false;
  }

  // success!
  LOG("flash: wrote %ld bytes", written);
  memmove(&fd->buf[0], &fd->buf[written], fd->len - written);
  fd->len -= written;
  return true;
}

static void flash_dumper_periodic_flush(void *data) {
  flash_dumper_t *fd = (flash_dumper_t *)data;
  schedule_us(DUMP_PERIOD_MSEC * 1000, flash_dumper_periodic_flush, fd);
  if (fd->len > 0) {
    if (!flash_dumper_flush(fd, fd->len)) {
      LOG("warning: sd card failure");
    }
  }
}

void flash_dumper_write(flash_dumper_t *fd, const void *buf, int len) {
  // prepend all lines with the current time in milliseconds and a comma
  char prefix[50];
  uint32_t sec, usec;
  wallclock_get_uptime(&fd->wallclock, &sec, &usec);
  int prefix_len = sprintf(prefix, "%ld,", sec * 1000 + usec / 1000);

  // compute total length of the string: the prefix, the string we were passed,
  // and one extra for the trailing \n
  uint32_t len_with_extras = 0;
  len_with_extras += prefix_len;
  len_with_extras += len;
  len_with_extras += 1;

  // make sure there's sufficient space
  if (fd->len + len_with_extras > sizeof(fd->buf)) {
    LOG("ERROR: Flash buf overflow!");
    fd->len = 0;
  } else {
    // append prefix, message and \n to buffer
    memcpy(&fd->buf[fd->len], prefix, prefix_len);
    fd->len += prefix_len;
    memcpy(&fd->buf[fd->len], buf, len);
    fd->len += len;
    fd->buf[fd->len++] = '\n';
  }

  // write the entry to the console for debug
  log_write(&fd->buf[fd->len - len_with_extras], len_with_extras);

  // if we've reached 2 complete FAT sectors, write
  while (fd->len >= WRITE_INCREMENT) {
    if (!flash_dumper_flush(fd, WRITE_INCREMENT)) {
      return;
    }
  }
}

void flash_dumper_print(flash_dumper_t *fd, const char *s) {
  flash_dumper_write(fd, s, strlen(s));
}
