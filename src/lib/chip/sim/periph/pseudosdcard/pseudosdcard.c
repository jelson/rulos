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

#include <stdio.h>  // SIM-only
#include "core/logging.h"   // assert
#include "periph/pseudosdcard/pseudosdcard.h"

#define DISK_IMAGE_PATH "../../../src/util/audio/sdcard.img"
#define SECTOR_SIZE (512)
static FILE* disk_fp = NULL;

DSTATUS disk_initialize(BYTE pdrv) {
  disk_fp = fopen(DISK_IMAGE_PATH, "rb");
  if (disk_fp == NULL) {
    return RES_ERROR;
  }
  return RES_OK;
}

DSTATUS disk_status(BYTE pdrv) {
  return 0 | STA_PROTECT;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
  int rc;
  rc = fseek(disk_fp, sector * SECTOR_SIZE, SEEK_SET);
  assert(rc==0);
  rc = fread(buff, SECTOR_SIZE, count, disk_fp);
  assert(rc==count);
  LOG("read sector %d for count %d", sector, count);
  return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
  return RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
  return RES_ERROR;
}
