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

ROCKET_ROOT := ../../..

PLATFORMS := sim
PERIPHERALS := rocket 7seg_panel twi display_rtc hpam audio adc rasters joystick uart input_controller remote_keyboard
AVR_CFLAGS := -DLOG_TO_SERIAL=UART0
#ARM_CFLAGS := -DLOG_TO_SERIAL=UART0

# Change SIM_CFLAGS to NETROCKET to debug remote bbufs
SIM_PERIPHERALS += keypad
SIM_CFLAGS := -DBOARDCONFIG_ROCKET0
#SIM_CFLAGS := -DBOARDCONFIG_NETROCKET

#MCU := atmega8
#MCU := atmega328p
#BOARD := PCB11

## Uncomment this block for classic LPEM talking to an EPB
PLATFORMS += avr
AVR_CFLAGS := -DBOARDCONFIG_ROCKET0
MCU := atmega1284p
BOARD := LPEM
PERIPHERALS += 7seg_panel_hardware keypad

## Uncomment this block for LPEM2 with remote bbufs
#PLATFORMS += avr
#MCU := atmega1284
#BOARD := LPEM2
#AVR_CFLAGS := -DBOARDCONFIG_NETROCKET

## Uncomment this block for test rocket on ARM
#PLATFORMS += arm
##ARM_CHIP := lpc1114-102
#ARM_CHIP := lpc1115
#ARM_CFLAGS += -DBOARDCONFIG_NETROCKET
#BOARD := ARMPEM_REVA

##################################

TARGET := rocket

TARGET_OBJECTS := \
	rocket.o

include $(ROCKET_ROOT)/Makefile.common
