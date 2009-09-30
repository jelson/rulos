COMMON_OBJECTS = \
	board_buffer.o \
	calculator.o \
	clock.o \
	cpumon.o \
	cursor.o \
	display_compass.o \
	display_controller.o \
	display_docking.o \
	display_gratuitous_graph.o \
	display_rtc.o \
	display_scroll_msg.o \
	drift_anim.o \
	focus.o \
	heap.o \
	idle_display.o \
	input_controller.o \
	knob.o \
	labeled_display.o \
	numeric_input.o \
	queue.o \
	random.o \
	rocket.o \
	util.o

.PHONY: all
all: sim-all avr-all

clean: sim-clean avr-clean

include Makefile.sim
include Makefile.avr

