# Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

ROCKET_ROOT := ../..

PLATFORMS := arm #avr
#MCU := atmega328
TARGET := nulltest
BOARD := GENERIC

ARM_CHIP := stm32g030x6
#ARM_CHIP := stm32g031x8
#ARM_CHIP := stm32f303xb
#ARM_CHIP := stm32f303x8
#ARM_CHIP := stm32f031x6
#ARM_CHIP := stm32f103x8
#ARM_CHIP := lpc1114-102
#AVR_CFLAGS := -DCRYSTAL=12000000 -DTIMING_DEBUG
#ALL_PLATFORM_CFLAGS := -DLOG_TO_SERIAL=UART0
PERIPHERALS := #uart

TARGET_OBJECTS := \
	nulltest.o

include $(ROCKET_ROOT)/Makefile.common
