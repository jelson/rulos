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

target: avr-all wandhost

wandhost: wandhost.c
	gcc -o wandhost -g wandhost.c

ROCKET_ROOT := ../..

MCU := atmega328p
BOARD := CUSTOM
TARGET := magicwand

# Set twi to 500mhz (assuming 8 mhz processor)
EXTRA_AVR_CFLAGS += -DCUSTOM_TWBR=32

TARGET_OBJECTS := \
	uart.o \
	magicwand.o 


include $(ROCKET_ROOT)/Makefile.common

