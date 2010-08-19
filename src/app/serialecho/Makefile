ROCKET_ROOT := ../..

MCU := atmega328p
BOARD := CUSTOM
TARGET := serialecho

# Set twi to 500mhz (assuming 8 mhz processor)
EXTRA_AVR_CFLAGS += -DCUSTOM_TWBR=32

TARGET_OBJECTS := \
	uart.o \
	serialecho.o 


include $(ROCKET_ROOT)/Makefile.common

