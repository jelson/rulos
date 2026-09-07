#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/rulos.h"
#include "periph/fatfs/diskio.h"

#define SECTOR_SIZE (512)
#define NUM_SECTORS (256 * 1024)

#define SDSIZE (SECTOR_SIZE * NUM_SECTORS)

char sdbuf[SDSIZE] = {};

DSTATUS disk_status(BYTE pdrv) {
  return 0;
}

DSTATUS disk_initialize(BYTE pdrv) {
  return 0;
}

static bool valid_range(DWORD sector, UINT count) {
  // Check in sectors before multiplying, including ranges ending exactly at capacity.
  return count > 0 && sector < NUM_SECTORS && count <= NUM_SECTORS - sector;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count) {
  LOG("read %d from sector %d", count, sector);
  if (!valid_range(sector, count)) {
    return RES_PARERR;
  }
  memcpy(buff, &sdbuf[(size_t)sector * SECTOR_SIZE], (size_t)count * SECTOR_SIZE);
  return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count) {
  LOG("write %d to sector %d", count, sector);
  if (!valid_range(sector, count)) {
    return RES_PARERR;
  }
  memcpy(&sdbuf[(size_t)sector * SECTOR_SIZE], buff, (size_t)count * SECTOR_SIZE);
  return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
  LOG("got ioctl %d", cmd);
  switch (cmd) {
    case CTRL_SYNC:
      break;
    case GET_SECTOR_COUNT:
      *(DWORD *)buff = NUM_SECTORS;
      break;
    case GET_BLOCK_SIZE:
      *(DWORD *)buff = SECTOR_SIZE;
      break;
    default:
      assert(FALSE);
  }
  return RES_OK;
}
