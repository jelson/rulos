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
PERIPHERALS := uart 7seg_panel twi joystick input_controller rocket rasters audio keypad
	# TODO rocket for screen4
AVR_CFLAGS := -DLOG_TO_SERIAL=UART0

## Uncomment this block for LPEM2
MCU := atmega1284
BOARD := LPEM
AVR_CFLAGS += -DBOARDCONFIG_UNIROCKET
# Change SIM_CFLAGS to -DBOARDCONFIG_UNIROCKET to debug remote bbufs
# Change SIM_CFLAGS to -DBOARDCONFIG_UNIROCKET_LOCALSIM to display locally
SIM_CFLAGS := -DBOARDCONFIG_UNIROCKET_LOCALSIM
SIM_PERIPHERALS := keypad 7seg_panel_hardware

TARGET := snaketest

TARGET_OBJECTS := \
	snaketest.o \
	snakegame.o

include $(ROCKET_ROOT)/Makefile.common
