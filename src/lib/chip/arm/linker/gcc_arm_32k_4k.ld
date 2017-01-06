/* Linker script to configure memory regions. */
MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x08000   /* 32k */
  RAM (rwx)  : ORIGIN = 0x10000000, LENGTH = 0x01000   /* 4k */

  /* specify RAM1 and RAM2 regions as zero length to stop them being used */

  RAM1(rwx) : ORIGIN = 0x00000000, LENGTH = 0k
  RAM2(rwx) : ORIGIN = 0x00000000, LENGTH = 0k
}

INCLUDE gcc_flash_common.ld
