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

everything: avr-all serialhost

serialhost: serialhost.c
	gcc -o $@ -g $^

PLATFORMS := avr
MCU := atmega8
BOARD := CUSTOM
TARGET := serialecho

# Set twi to 500mhz (assuming 8 mhz processor)
EXTRA_AVR_CFLAGS += -DCUSTOM_TWBR=32

TARGET_OBJECTS := \
	serialecho.o 


include $(ROCKET_ROOT)/Makefile.common

