#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "core/rulos.h"
#include "periph/fatfs/diskio.h"

#define SECTOR_SIZE (512)
#define NUM_SECTORS (256*1024)

#define SDSIZE (SECTOR_SIZE*NUM_SECTORS)

char sdbuf[SDSIZE] = {};

DSTATUS disk_status(BYTE pdrv) {
  return 0;
}

DSTATUS disk_initialize (BYTE pdrv) {
  return 0;
}

DRESULT disk_read (BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
  LOG("read %d from sector %d", count, sector);
  int addr = sector * SECTOR_SIZE;
  count *= SECTOR_SIZE;
  assert(addr >= 0);
  assert(addr < SDSIZE);
  assert(addr + count < SDSIZE);
  //LOG("-->read %d from sector %d", count, addr);
  memcpy(buff, &sdbuf[addr], count);
  return RES_OK;
}

DRESULT disk_write (BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
  LOG("write %d to sector %d", count, sector);
  int addr = sector * SECTOR_SIZE;
  count *= SECTOR_SIZE;
  assert(addr >= 0);
  assert(addr < SDSIZE);
  assert(addr + count < SDSIZE);
  //LOG("-->write %d to sector %d", count, addr);
  memcpy(&sdbuf[addr], buff, count);
  return RES_OK;
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff) {
  LOG("got ioctl %d", cmd);
  switch (cmd) {
  case CTRL_SYNC:
    break;
  case GET_SECTOR_COUNT:
    *(DWORD*)buff = NUM_SECTORS;
    break;
  case GET_BLOCK_SIZE:
    *(DWORD*)buff = SECTOR_SIZE;
    break;
  default:
    assert(FALSE);
  }
  return RES_OK;
}
