# Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
# (jelson@gmail.com).
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

PLATFORMS := arm
ARM_CHIP := stm32f303x8
TARGET := max3421test
BOARD := GENERIC
ALL_PLATFORM_CFLAGS := -DLOG_TO_SERIAL=UART0
PERIPHERALS := uart max3421e spi

TARGET_OBJECTS := \
	max3421test.o

include $(ROCKET_ROOT)/Makefile.common
