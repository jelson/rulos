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
PERIPHERALS := uart
MCU := atmega1284p
#MCU := atmega328p
BOARD := CUSTOM

TARGET := salwow
EXTRA_AVR_CFLAGS += -DASSERT_TO_SERIAL
EXTRA_AVR_LDFLAGS += -lm

TARGET_OBJECTS := \
	salwow.o \
	gpsinput.o \
	mark_point.o \
	navigation.o \
	control.o \
	leds.o \
	rudder.o \
	motors.o \

include $(ROCKET_ROOT)/Makefile.common

