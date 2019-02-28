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

everything: avr-all serialhost

serialhost: serialhost.c
	gcc -o $@ -g $^

PLATFORMS := avr
PERIPHERALS := uart
MCU := atmega8
BOARD := GENERIC
TARGET := serialecho

# Set twi to 500mhz (assuming 8 mhz processor)
AVR_CFLAGS += -DCUSTOM_TWBR=32

TARGET_OBJECTS := \
	serialecho.o 


include $(ROCKET_ROOT)/Makefile.common

