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

#pragma once

// RULOS DFU firmware-update facility (STM32), shared by any app that
// uses the USB CDC peripheral.
//
// The application does NOT implement DFU flash programming. It only
// hands off to the chip's built-in ROM system bootloader, which itself
// speaks USB DFU (it enumerates as ST 0483:DF11). The end-to-end host
// flow with the standard dfu-util tool is:
//
//   1. `dfu-util -e` sends a USB DFU_DETACH to the device's DFU runtime
//      interface (advertised in the USB CDC configuration descriptor).
//   2. The USB layer calls rulos_dfu_enter_bootloader(), which stores a
//      magic value in warm-reset-surviving RAM (the linker's .noinit
//      section) and performs a system reset.
//   3. The very first statement of rulos_hal_init() calls
//      rulos_dfu_check_and_jump(); seeing the magic, it clears it and
//      jumps to the ROM bootloader.
//   4. `dfu-util -D firmware.bin` then writes flash via the ROM DFU
//      device; ":leave" makes the ROM start the new image.
//
// A wrong jump or a broken USB descriptor only disables this USB path:
// SWD reflash (Black Magic Probe) always recovers the device.

// Record the reboot magic and NVIC_SystemReset(). Does not return.
void rulos_dfu_enter_bootloader(void);

// Call as the very first statement of rulos_hal_init(), before
// HAL_Init() and before any other RAM is touched. If the reboot magic
// is set, clears it and jumps into the system ROM bootloader (does not
// return). Otherwise returns immediately and boot proceeds normally.
void rulos_dfu_check_and_jump(void);
