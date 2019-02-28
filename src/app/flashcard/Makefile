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

PLATFORMS := avr
PERIPHERALS := uart led_matrix_single keypad lcd_12232 eeprom input_controller \
	audio sdcard spi 
#MCU := atmega8
MCU := atmega1284p
BOARD := FLASHCARD
#AVR_CFLAGS += -DCRYSTAL=20000000
ALL_PLATFORM_CFLAGS := -DBOARDCONFIG_DEFAULT

TARGET := flashcard

TARGET_OBJECTS := \
	flashcard.o \

include $(ROCKET_ROOT)/Makefile.common

$(AVR_OBJDIR)/app/flashcard.o: flashcard.c flashcard.c $(AVR_OBJDIR)/starbitmaps.ch

%/starbitmaps.ch: star/*.png star/ledmaps.py
	star/ledmaps.py $@ star

