 #
 # This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 # System -- the free, open-source operating system for microcontrollers.
 #
 # Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 # May 2009.
 #
 # This operating system is in the public domain.  Copyright is waived.
 # All source code is completely free to use by anyone for any purpose
 # with no restrictions whatsoever.
 #
 # For more information about the project, see: www.jonh.net/rulos
 #

ROCKET_ROOT := ../..

PLATFORMS := avr
PERIPHERALS := rasters uart led_matrix_single keypad lcd_12232 eeprom input_controller 7seg_panel rocket hpam
#MCU := atmega8
MCU := atmega1284p
BOARD := FLASHCARD
#EXTRA_AVR_CFLAGS += -DCRYSTAL=20000000

TARGET := flashcard

TARGET_OBJECTS := \
	flashcard.o \

include $(ROCKET_ROOT)/Makefile.common

$(AVR_OBJDIR)/app/flashcard.o: flashcard.c flashcard.c $(AVR_OBJDIR)/starbitmaps.ch

%/starbitmaps.ch: star/*.png star/ledmaps.py
	star/ledmaps.py $@ star

