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

PLATFORMS := sim
PERIPHERALS := rocket 7seg_panel twi display_rtc hpam audio adc rasters joystick uart input_controller remote_keyboard
ALL_PLATFORM_CFLAGS := -DLOG_TO_SERIAL=UART0 -DSCHEDULER_CAPACITY=64

## SIM CFLAGS can be one of:
## -DBOARDCONFIG_UNIROCKET_LOCALSIM, to view boards in local sim
## -DBOARDCONFIG_UNIROCKET, to debug remote bbuf protocol/dongles
#SIM_CFLAGS := -DBOARDCONFIG_UNIROCKET
SIM_CFLAGS := -DBOARDCONFIG_UNIROCKET_LOCALSIM
SIM_PERIPHERALS += keypad

## Uncomment this block for LPEM2
#PLATFORMS += avr
#MCU := atmega1284
#BOARD := LPEM2
#AVR_CFLAGS += -DBOARDCONFIG_UNIROCKET

## Uncomment this block for ARM test
PLATFORMS += arm
ARM_CHIP := stm32f103x8
ARM_CFLAGS += -DBOARDCONFIG_UNIROCKET
BOARD := STMPEM_REVA

TARGET := unirocket

TARGET_OBJECTS := \
	unirocket.o

include $(ROCKET_ROOT)/Makefile.common
