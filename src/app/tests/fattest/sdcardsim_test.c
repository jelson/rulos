#include "periph/sdcardsim/sdcardsim.c"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_boundary(void) {
  DWORD sectors;
  assert(disk_ioctl(0, GET_SECTOR_COUNT, &sectors) == RES_OK);
  BYTE expected[1024], actual[sizeof(expected)];
  for (unsigned i = 0; i < sizeof(expected); i++) {
    expected[i] = (BYTE)(i ^ (i >> 8));
  }
  assert(disk_write(0, expected, sectors - 1, 1) == RES_OK);
  assert(disk_read(0, actual, sectors - 1, 1) == RES_OK);
  assert(memcmp(expected, actual, 512) == 0);
  assert(disk_write(0, expected, sectors - 2, 2) == RES_OK);
  assert(disk_read(0, actual, sectors - 2, 2) == RES_OK);
  assert(memcmp(expected, actual, sizeof(expected)) == 0);

  const struct {
    DWORD sector;
    UINT count;
  } invalid[] = {
      {0, 0},          {sectors, 1},  {sectors - 1, 2}, {UINT32_MAX, 1},
      {0, UINT32_MAX}, {0, 1U << 23}, {1U << 23, 1},
  };
  for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
    assert(disk_read(0, actual, invalid[i].sector, invalid[i].count) == RES_PARERR);
    assert(disk_write(0, expected, invalid[i].sector, invalid[i].count) == RES_PARERR);
  }
  assert(disk_read(0, actual, sectors - 2, 2) == RES_OK);
  assert(memcmp(expected, actual, sizeof(expected)) == 0);
}

static void test_filesystem(void) {
  FATFS fs;
  BYTE work[4096], expected[1537], actual[sizeof(expected)];
  for (unsigned i = 0; i < sizeof(expected); i++) {
    expected[i] = (BYTE)(i ^ (i >> 8));
  }
  const MKFS_PARM options = {.fmt = FM_FAT32, .au_size = 1024};
  assert(f_mount(&fs, "", 0) == FR_OK);
  assert(f_mkfs("", &options, work, sizeof(work)) == FR_OK);
  FIL file;
  UINT written, read;
  const char *path = "storage-regression.bin";
  assert(f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);
  assert(f_write(&file, expected, sizeof(expected), &written) == FR_OK);
  assert(written == sizeof(expected));
  assert(f_sync(&file) == FR_OK);
  assert(f_close(&file) == FR_OK);
  assert(f_mount(NULL, "", 0) == FR_OK);
  assert(f_mount(&fs, "", 1) == FR_OK);
  assert(f_open(&file, path, FA_READ) == FR_OK);
  assert(f_read(&file, actual, sizeof(actual), &read) == FR_OK);
  assert(read == sizeof(actual) && memcmp(expected, actual, sizeof(actual)) == 0);
  assert(f_close(&file) == FR_OK);
  assert(f_mount(NULL, "", 0) == FR_OK);
}

int main(void) {
  test_boundary();
  test_filesystem();
  puts("SD simulator: last-sector I/O and invalid ranges passed");
  puts("FatFs: format, multi-sector file write, sync, remount, and readback passed");
}
