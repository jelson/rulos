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

#MCU := atmega8
MCU := atmega328p
BOARD := CUSTOM

TARGET := uartnetworktest

TARGET_OBJECTS := \
	uartnetworktest.o

uartnetwork_all: all uartnetworkhost hostaudio

HOSTTEST_OBJS=uartnetworkhost.o net_compute_checksum.o host_uart_network.o

HOSTAUDIO_OBJS=hostaudio.o linear2ulaw.o net_compute_checksum.o host_uart_network.o host_audio_capture.o

include $(ROCKET_ROOT)/Makefile.common

uartnetworkhost: $(patsubst %.o,$(SIM_OBJDIR)/%.o,$(HOSTTEST_OBJS))
	gcc -g $^ -o $@ -I$(ROCKET_ROOT)/include -Wall

hostaudio: $(patsubst %.o,$(SIM_OBJDIR)/%.o,$(HOSTAUDIO_OBJS))
	gcc -g $^ -o $@ -I$(ROCKET_ROOT)/include -Wall -lasound
