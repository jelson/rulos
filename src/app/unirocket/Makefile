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
PERIPHERALS := rocket 7seg_panel twi display_rtc hpam audio adc rasters joystick uart input_controller remote_keyboard
AVR_CFLAGS := -DLOG_TO_SERIAL=UART0

## Uncomment this block for local simulator
#BOARD := LPEM
#PERIPHERALS += 7seg_panel_hardware keypad
#ALL_PLATFORM_CFLAGS := -DBOARDCONFIG_UNIROCKET_LOCALSIM

## Uncomment this block for LPEM2
MCU := atmega1284
BOARD := LPEM2
AVR_CFLAGS := -DBOARDCONFIG_UNIROCKET
# Change SIM_CFLAGS to -DBOARDCONFIG_UNIROCKET to debug remote bbufs
SIM_CFLAGS := -DBOARDCONFIG_UNIROCKET_LOCALSIM
SIM_PERIPHERALS := keypad

TARGET := unirocket

TARGET_OBJECTS := \
	unirocket.o

include $(ROCKET_ROOT)/Makefile.common
