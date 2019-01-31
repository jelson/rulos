/* Linker script to configure memory regions. */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 512K
  RAM (rwx) : ORIGIN = 0x10000000, LENGTH =  64K

  RAM1(rwx) : ORIGIN = 0x20000000, LENGTH = 16k
  RAM2(rwx) : ORIGIN = 0x20004000, LENGTH = 16k
}

INCLUDE gcc_flash_common.ld
