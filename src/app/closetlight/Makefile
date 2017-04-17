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
MCU := attiny45
TARGET := closetlight
BOARD := GENERIC
PERIPHERALS := usi_serial

EXTRA_AVR_CFLAGS := -DTIMING_DEBUG_PIN=GPIO_B0 -DSCHEDULER_CAPACITY=2 -DSCHEDULER_NOW_QUEUE_CAPACITY=2

TARGET_OBJECTS := \
	closetlight.o

include $(ROCKET_ROOT)/Makefile.common

