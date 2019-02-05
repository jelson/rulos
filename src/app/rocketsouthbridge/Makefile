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
PERIPHERALS := quadknob twi input_controller keypad remote_keyboard uart
MCU := atmega328p
BOARD := SOUTHBRIDGE
AVR_CFLAGS := -DLOG_TO_SERIAL=UART0

TARGET := rocketsouthbridge

TARGET_OBJECTS := \
	rocketsouthbridge.o

include $(ROCKET_ROOT)/Makefile.common

