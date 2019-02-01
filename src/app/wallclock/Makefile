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

PLATFORMS := avr sim #arm
PERIPHERALS = 7seg_panel 7seg_panel_hardware uart
ALL_PLATFORM_CFLAGS := -DBOARDCONFIG_WALLCLOCK
MCU := atmega8
TARGET := wallclock
BOARD := PCB11

ARM_CHIP := lpc1114-102

TARGET_OBJECTS := \
	wallclock.o \


include $(ROCKET_ROOT)/Makefile.common

