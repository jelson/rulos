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

PLATFORMS := avr sim
PERIPHERALS := uart 7seg_panel twi joystick input_controller rocket rasters audio keypad
	# TODO rocket for screen4
AVR_CFLAGS := -DLOG_TO_SERIAL=UART0

## Uncomment this block for LPEM2
MCU := atmega1284
BOARD := LPEM
AVR_CFLAGS += -DBOARDCONFIG_UNIROCKET
# Change SIM_CFLAGS to -DBOARDCONFIG_UNIROCKET to debug remote bbufs
# Change SIM_CFLAGS to -DBOARDCONFIG_UNIROCKET_LOCALSIM to display locally
SIM_CFLAGS := -DBOARDCONFIG_UNIROCKET_LOCALSIM
SIM_PERIPHERALS := keypad 7seg_panel_hardware

TARGET := snaketest

TARGET_OBJECTS := \
	snaketest.o

include $(ROCKET_ROOT)/Makefile.common
