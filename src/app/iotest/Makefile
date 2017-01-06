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

all: avr-all

ROCKET_ROOT := ../..

PLATFORMS := avr
PERIPHERALS := uart 7seg_panel keypad rocket input_controller hpam audio sdcard spi joystick rasters display_rtc
MCU := atmega1284p
TARGET := iotest
BOARD := LPEM

TARGET_OBJECTS := \
	iotest.o


include $(ROCKET_ROOT)/Makefile.common

