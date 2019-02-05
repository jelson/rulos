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

PLATFORMS := arm # avr
MCU := atmega328
TARGET := nulltest
BOARD := GENERIC

ARM_CHIP := stm32f103x8
#ARM_CHIP := lpc1114-102
#AVR_CFLAGS := -DCRYSTAL=12000000 -DTIMING_DEBUG
AVR_CFLAGS := -DLOG_TO_SERIAL=UART0
PERIPHERALS := uart

TARGET_OBJECTS := \
	nulltest.o

include $(ROCKET_ROOT)/Makefile.common

