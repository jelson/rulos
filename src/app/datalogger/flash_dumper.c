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

#include "core/rtc.h"
#include "core/rulos.h"

void flash_dumper_init(flash_dumper_t *fd) {
  const char *filename = "log.txt";

  memset(fd, 0, sizeof(*fd));

  rtc_init(&fd->rtc);

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
  fd->ok = true;
}

void flash_dumper_write(flash_dumper_t *fd, const void *buf, int len) {
  // prepend all lines with the current time in milliseconds and a comma
  char prefix[50];
  uint32_t sec, usec;
  rtc_get_uptime(&fd->rtc, &sec, &usec);
  int prefix_len = sprintf(prefix, "%ld,", sec * 1000 + usec / 1000);

  // compute total length of the string: the prefix, the string we were passed,
  // and one extra for the trailing \n
  int len_with_extras = 0;
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
    uint32_t written;
    int retval = f_write(&fd->fp, fd->buf, WRITE_INCREMENT, &written);
    if (retval != FR_OK) {
      LOG("couldn't write to sd card: got retval of %d", retval);
      fd->ok = false;
      return;
    }
    if (written == 0) {
      LOG("error writing to SD card: nothing written");
      fd->ok = false;
      return;
    }
    retval = f_sync(&fd->fp);
    if (retval != FR_OK) {
      fd->ok = false;
      return;
    }

    // success!
    LOG("flash: wrote %ld bytes", written);
    memmove(&fd->buf[0], &fd->buf[written], fd->len - written);
    fd->len -= written;
  }
}

void flash_dumper_print(flash_dumper_t *fd, char *s) {
  flash_dumper_write(fd, s, strlen(s));
}
