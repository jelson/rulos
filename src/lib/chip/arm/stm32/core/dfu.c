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

#include "core/dfu.h"

#include <stdint.h>

#include "core/hardware.h"  // pulls in stm32.h -> CMSIS (SCB, NVIC, MSP)

// Stored in the linker's .noinit section: not in .bss (so the startup
// zero loop never clears it) and NOLOAD (no flash LMA), so its value
// is whatever SRAM held -- which is retained across the warm reset
// that NVIC_SystemReset() performs. `used` keeps it even though only
// the two functions below touch it; `volatile` because it is written
// on one side of a reset and read on the other.
#define DFU_REBOOT_MAGIC 0xDF00B007u
static volatile uint32_t __attribute__((section(".noinit"), used)) g_dfu_reboot_magic;

// System (ROM) bootloader entry address per chip family. This is the
// fixed address of the factory bootloader's vector table; *base is its
// initial MSP and *(base+4) its reset entry. The ROM bootloader speaks
// USB DFU natively (enumerates as ST 0483:DF11). Addresses are from
// ST AN2606 / the CMSIS device headers (H5: FLASH_SYSTEM_BASE_NS).
#if defined(RULOS_ARM_stm32h5)
// On STM32H5 the bootloader's vector table is NOT at the system-flash
// region start (FLASH_SYSTEM_BASE_NS = 0x0BF80000); it sits at a
// sub-family-specific offset. ST's own examples pin it: NUCLEO-H503RB
// uses 0x0BF87000; NUCLEO-H533RE (the H52x/H53x sub-family, which
// STM32H523 belongs to) uses 0x0BF97000. Jumping to the region start
// lands on non-vector bytes -> garbage SP/PC -> HardFault. Select by
// CMSIS device macro and fail the build loudly for an unrecognized H5
// rather than ever guessing an address (a wrong guess bricks USB until
// an SWD reflash).
#define DFU_HAVE_SYSMEM 1
#if defined(STM32H523xx) || defined(STM32H533xx) || defined(STM32H562xx) || \
    defined(STM32H563xx) || defined(STM32H573xx)
#define DFU_SYSMEM_BASE 0x0BF97000u
#elif defined(STM32H503xx)
#define DFU_SYSMEM_BASE 0x0BF87000u
#else
#error \
    "dfu: unknown STM32H5 device -- add its ROM bootloader \
base address (see ST's NUCLEO-H5xx ROT examples: BOOTLOADER_BASE)."
#endif
#elif defined(RULOS_ARM_stm32g4) || defined(RULOS_ARM_stm32g0) || defined(RULOS_ARM_stm32f3)
#define DFU_HAVE_SYSMEM 1
#define DFU_SYSMEM_BASE 0x1FFF0000u
// The legacy ROM bootloader on these parts is linked to run at
// 0x00000000 and reaches its own vector table and data through that
// alias. A software jump that leaves main flash mapped at 0x00000000
// runs a few bootloader instructions and then falls through to the
// application instead of staying in USB DFU. Remap system memory to
// 0x00000000 first. (The H5's M33 bootloader is position-independent
// and needs no remap -- hence none in that branch.) SYSCFG is
// clock-gated, so enable it with the mandatory readback before
// touching the remap register. Implemented and hardware-validated on
// G4. G0/F3 share this legacy ROM bootloader and would need the same
// remap (against SYSCFG->CFGR1 rather than MEMRMP) before their DFU
// path works -- not wired up here.
#if defined(RULOS_ARM_stm32g4)
#define DFU_REMAP_SYSMEM()                                                                  \
  do {                                                                                      \
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;                                                   \
    (void)RCC->APB2ENR;                                                                     \
    SYSCFG->MEMRMP = (SYSCFG->MEMRMP & ~SYSCFG_MEMRMP_MEM_MODE) | SYSCFG_MEMRMP_MEM_MODE_0; \
  } while (0)
#endif
#else
// Facility still links (so app code can call it unconditionally), but
// this chip has no known software-jumpable USB-DFU ROM bootloader: the
// request is ignored and boot proceeds normally.
#define DFU_HAVE_SYSMEM 0
#endif

void rulos_dfu_enter_bootloader(void) {
  g_dfu_reboot_magic = DFU_REBOOT_MAGIC;
  __DSB();
  NVIC_SystemReset();
  while (1) {
  }  // unreachable
}

void rulos_dfu_check_and_jump(void) {
  if (g_dfu_reboot_magic != DFU_REBOOT_MAGIC) {
    return;
  }
  // Consume the request before jumping so a crash in (or a normal exit
  // from) the bootloader can't trap us in a reboot loop.
  g_dfu_reboot_magic = 0;

#if DFU_HAVE_SYSMEM
  // We run as the very first thing in rulos_hal_init(): SystemInit()
  // has executed but HAL_Init(), the clock config, caches, the MPU,
  // and every peripheral are still at reset state -- the cleanest
  // possible environment for the ROM bootloader. We mask interrupts,
  // remap system memory to 0x00000000 where required, point the vector
  // table at the bootloader, load its stack pointer, and branch to its
  // reset entry.
  const uint32_t base = DFU_SYSMEM_BASE;
  const uint32_t boot_sp = *(volatile uint32_t*)base;
  const uint32_t boot_pc = *(volatile uint32_t*)(base + 4u);

  __disable_irq();
  SysTick->CTRL = 0;
  SysTick->VAL = 0;
#ifdef DFU_REMAP_SYSMEM
  DFU_REMAP_SYSMEM();
#endif
  SCB->VTOR = base;

  __set_MSP(boot_sp);
#if defined(__ARM_ARCH_8M_MAIN__) || defined(__ARM_ARCH_8M_BASE__)
  // Cortex-M33 (STM32H5): the application linker sets MSPLIM; the
  // bootloader's stack lives below it, so clear the limit or the very
  // first push faults.
  __set_MSPLIM(0);
#endif
  __DSB();
  __ISB();

  ((void (*)(void))boot_pc)();
  while (1) {
  }  // unreachable
#endif
}
