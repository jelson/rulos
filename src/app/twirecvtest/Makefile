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
PERIPHERALS := twi 7seg_panel
#MCU := atmega328p
#BOARD := PCB11
#MCU := atmega1284p
#BOARD := LPEM

MCU := atmega328
BOARD := DONGLE

TARGET := twirecvtest

TARGET_OBJECTS := \
	twirecvtest.o


include $(ROCKET_ROOT)/Makefile.common

