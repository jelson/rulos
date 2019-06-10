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

PLATFORMS := sim
PERIPHERALS := rocket 7seg_panel twi display_rtc hpam audio adc rasters joystick uart input_controller remote_keyboard quadknob
ALL_PLATFORM_CFLAGS := -DLOG_TO_SERIAL=UART0 -DSCHEDULER_CAPACITY=64

## SIM CFLAGS can be one of:
## -DBOARDCONFIG_UNIROCKET_LOCALSIM, to view boards in local sim
## -DBOARDCONFIG_UNIROCKET, to debug remote bbuf protocol/dongles
#SIM_CFLAGS := -DBOARDCONFIG_UNIROCKET
SIM_CFLAGS := -DBOARDCONFIG_UNIROCKET_LOCALSIM
SIM_PERIPHERALS += keypad joystick_adc

## Uncomment this block for LPEM2
#PLATFORMS += avr
#PERIPHERALS += joystick_adc
#MCU := atmega1284
#BOARD := LPEM2
#AVR_CFLAGS += -DBOARDCONFIG_UNIROCKET

## Uncomment this block for ARM-based LPEM III
PLATFORMS += arm
ARM_PERIPHERALS += max3421e spi joystick_usb
ARM_CHIP := stm32f303x8
ARM_CFLAGS += -DBOARDCONFIG_UNIROCKET
BOARD := STMPEM_REVA

TARGET := unirocket

TARGET_OBJECTS := \
	unirocket.o

include $(ROCKET_ROOT)/Makefile.common
