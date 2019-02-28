# Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

ROCKET_ROOT := ../..

#MCU := atmega8
MCU := atmega328p
BOARD := CUSTOM

TARGET := uartnetworktest

TARGET_OBJECTS := \
	uartnetworktest.o

uartnetwork_all: uartnetworkhost hostaudio

HOSTTEST_OBJS=uartnetworkhost.o net_compute_checksum.o host_uart_network.o

HOSTAUDIO_OBJS=hostaudio.o linear2ulaw.o net_compute_checksum.o host_uart_network.o host_audio_capture.o

include $(ROCKET_ROOT)/Makefile.common

uartnetworkhost: $(patsubst %.o,$(SIM_OBJDIR)/%.o,$(HOSTTEST_OBJS))
	gcc -g $^ -o $@ -I$(ROCKET_ROOT)/include -Wall

hostaudio: $(patsubst %.o,$(SIM_OBJDIR)/%.o,$(HOSTAUDIO_OBJS))
	gcc -g $^ -o $@ -I$(ROCKET_ROOT)/include -Wall -lasound
