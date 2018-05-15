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
MCU := attiny84a
PERIPHERALS := keypad usi_twi_slave
BOARD := CUSTOM
TARGET := keyshield

TARGET_OBJECTS := \
	keyshield.o

KEYPAD_TYPE=OOTDTY
#KEYPAD_TYPE=FUTURLEC

EXTRA_AVR_CFLAGS := -DSCHEDULER_CAPACITY=4 -DSCHEDULER_NOW_QUEUE_CAPACITY=4 -D$(KEYPAD_TYPE)_KEYPAD

include $(ROCKET_ROOT)/Makefile.common

