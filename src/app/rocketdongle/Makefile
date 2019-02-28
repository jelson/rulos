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
PERIPHERALS := twi 7seg_panel 7seg_panel_hardware uart
ALL_PLATFORM_CFLAGS := -DBOARDCONFIG_ROCKET1 -DDONGLE_BOARD_ID=11 -DLOG_TO_SERIAL=UART0

MCU := atmega328
BOARD := DONGLE_REVB

TARGET := rocketdongle

TARGET_OBJECTS := \
	rocketdongle.o


include $(ROCKET_ROOT)/Makefile.common

