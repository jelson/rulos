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
MCU := attiny44
#MCU := attiny84
BOARD := GENERIC
PERIPHERALS := usi_twi_master
AVR_CFLAGS := -DSCHEDULER_CAPACITY=3 -DSCHEDULER_NOW_QUEUE_CAPACITY=2

TARGET := bike_ornament
TARGET_OBJECTS := \
	bike_ornament_v2.o

include $(ROCKET_ROOT)/Makefile.common

