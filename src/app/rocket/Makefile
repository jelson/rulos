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
AVR_CFLAGS := -DLOG_TO_SERIAL=UART0

# Change SIM_CFLAGS to NETROCKET to debug remote bbufs
SIM_PERIPHERALS += keypad
SIM_CFLAGS := -DBOARDCONFIG_ROCKET0
#SIM_CFLAGS := -DBOARDCONFIG_NETROCKET

#MCU := atmega8
#MCU := atmega328p
#BOARD := PCB11

## Uncomment this block for classic LPEM talking to an EPB
#MCU := atmega1284p
#BOARD := LPEM
#PERIPHERALS += 7seg_panel_hardware keypad

## Uncomment this block for LPEM2 with remote bbufs
PLATFORMS += avr
MCU := atmega1284
BOARD := LPEM2
AVR_CFLAGS := -DBOARDCONFIG_NETROCKET

## Uncomment this block for test rocket on ARM
#PLATFORMS += arm
#ARM_CHIP_FAMILY := 11xx
#ARM_CFLAGS := -DBOARDCONFIG_NETROCKET
#BOARD := ARMPEM_REVA

##################################

TARGET := rocket

TARGET_OBJECTS := \
	rocket.o

include $(ROCKET_ROOT)/Makefile.common
