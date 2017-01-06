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

PLATFORMS := avr sim
PERIPHERALS := rocket 7seg_panel input_controller hpam audio sdcard spi joystick rasters display_rtc
MCU := atmega328p
BOARD := CUSTOM
TARGET := audiotest
#EXTRA_AVR_CFLAGS :=  -DCRYSTAL=12000000
EXTRA_AVR_LDFLAGS := -lm

TARGET_OBJECTS := \
	audiotest.o


include $(ROCKET_ROOT)/Makefile.common

