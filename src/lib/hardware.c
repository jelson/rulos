/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

/*
 * hardware.c: These functions are only needed for physical display hardware.
 *
 * This file is not compiled by the simulator.
 */

#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>

#include "rocket.h"
#include "hardware.h"
#include "board_defs.h"
#include "hal.h"


//////////////////////////////////////////////////////////////////////////////

static void init_pins()
{
#ifdef BOARDSEL0
	gpio_make_output(BOARDSEL0);
	gpio_make_output(BOARDSEL1);
	gpio_make_output(BOARDSEL2);
	gpio_make_output(DIGSEL0);
	gpio_make_output(DIGSEL1);
	gpio_make_output(DIGSEL2);
	gpio_make_output(SEGSEL0);
	gpio_make_output(SEGSEL1);
	gpio_make_output(SEGSEL2);
	gpio_make_output(DATA);
	gpio_make_output(STROBE);
#endif

#ifdef KEYPAD_COL0
	gpio_make_input(KEYPAD_COL0);
	gpio_make_input(KEYPAD_COL1);
	gpio_make_input(KEYPAD_COL2);
	gpio_make_input(KEYPAD_COL3);
#endif

#ifdef MCUatmega1284p
// Disable JTAG, which opens up PC2..PC5 as gpios (or their other features)
	MCUCR |= _BV(JTD);
	MCUCR |= _BV(JTD);
#endif // MCUatmega1284p
}


#ifndef BOARD_CUSTOM

static uint8_t segmentRemapTables[4][8] = {
#define SRT_SUBU	0
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
#define SRT_SDBU	1
// swap decimal point with segment H, which are the nodes reversed
// when an LED is mounted upside-down
	{ 0, 1, 2, 3, 4, 5, 7, 6 },
#define SRT_SUBD	2
	{ 3, 4, 5, 0, 1, 2, 6, 7 },
#define SRT_SDBD	3
	{ 3, 4, 5, 0, 1, 2, 7, 6 },
	};
typedef uint8_t SegmentRemapIndex;

typedef struct {
	r_bool				reverseDigits;
	SegmentRemapIndex	segmentRemapIndices[8];
} BoardRemap;

static BoardRemap boardRemapTables[] = {
#define BRT_SOLDERED_UP_BOARD_UP	0
	{ FALSE, { SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU }},
#define BRT_SOLDERED_DN_BOARD_DN	1
	{ TRUE,  { SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD }},
#define BRT_WALLCLOCK				2
//	{ FALSE, { SRT_SUBU, SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SUBU, SRT_SUBU }},
	{ FALSE, { SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SUBU }},
#define BRT_CHASECLOCK				3
	{ FALSE, { SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SUBU }},
};
typedef uint8_t BoardRemapIndex;

static BoardRemapIndex displayConfiguration[NUM_BOARDS];

uint16_t g_epb_delay_constant = 1;
void epb_delay()
{
	uint16_t delay;
	static volatile int x;
	for (delay=0; delay<g_epb_delay_constant; delay++)
	{
		x = x+1;
	}
}

/*
 * 0x1738 - 0x16ca new
 * 0x1708 - 0x16c8 old
 */
void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff)
{
	BoardRemap *br = &boardRemapTables[displayConfiguration[board]];
	uint8_t rdigit;
	if (br->reverseDigits)
	{
		rdigit = digit;
	}
	else
	{
		rdigit = NUM_DIGITS - 1 - digit;
	}
	uint8_t asegment = segmentRemapTables[br->segmentRemapIndices[rdigit]][segment];

	gpio_set_or_clr(DIGSEL0, rdigit & (1 << 0));	
	gpio_set_or_clr(DIGSEL1, rdigit & (1 << 1));	
	gpio_set_or_clr(DIGSEL2, rdigit & (1 << 2));	
	
	gpio_set_or_clr(SEGSEL0, asegment & (1 << 0));
	gpio_set_or_clr(SEGSEL1, asegment & (1 << 1));
	gpio_set_or_clr(SEGSEL2, asegment & (1 << 2));

	gpio_set_or_clr(BOARDSEL0, board & (1 << 0));
	gpio_set_or_clr(BOARDSEL1, board & (1 << 1));
	gpio_set_or_clr(BOARDSEL2, board & (1 << 2));

	/* sense reversed because cathode is common */
	gpio_set_or_clr(DATA, !onoff);
	
	epb_delay();

	gpio_clr(STROBE);

	epb_delay();

	gpio_set(STROBE);

	epb_delay();
}

#else
void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff)
{
	assert(FALSE);
}
#endif // BOARD_CUSTOM



/**************************************************************/
/*			 Interrupt-driven senor input                     */
/**************************************************************/
// jonh hardcodes this for a specific interrupt line. Not sure
// yet how to generalize; will do when needed, I guess.

#if 0
Handler sensor_interrupt_handler;

void sensor_interrupt_register_handler(Handler handler)
{
	sensor_interrupt_handler = handler;
}

ISR(INT0_vect)
{
	sensor_interrupt_handler();
}
#endif



void hal_init_joystick_button()
{
#if defined(JOYSTICK_TRIGGER)
	// Only PCB11 defines a joystick trigger line
	gpio_make_input(JOYSTICK_TRIGGER);
#else
	assert(FALSE);	// no joystick on this hardware! why are you trying to read it?
#endif
}

r_bool hal_read_joystick_button()
{
#if defined(JOYSTICK_TRIGGER)
	return gpio_is_clr(JOYSTICK_TRIGGER);
#else
	return FALSE;
#endif
}

/*************************************************************************************/


// disable interrupts, and return true if interrupts had been enabled.
uint8_t hal_start_atomic()
{
	uint8_t retval = SREG & _BV(SREG_I);
	cli();
	return retval;
}

// conditionally enable interrupts
void hal_end_atomic(uint8_t interrupt_flag)
{
	if (interrupt_flag)
		sei();
}

void hal_idle()
{
	// just busy-wait on microcontroller.
}

void hal_init(BoardConfiguration bc)
{
#ifndef BOARD_CUSTOM
	// This code is static per binary; could save some code space
	// by using #ifdefs instead of dynamic code.
	int i;
	for (i=0; i<NUM_BOARDS; i++)
	{
		displayConfiguration[i] = BRT_SOLDERED_UP_BOARD_UP;
	}
	switch (bc)
	{
		case bc_rocket0:
			displayConfiguration[0] = BRT_WALLCLOCK;
			displayConfiguration[3] = BRT_SOLDERED_DN_BOARD_DN;
			displayConfiguration[4] = BRT_SOLDERED_DN_BOARD_DN;
			break;
		case bc_rocket1:
			displayConfiguration[2] = BRT_SOLDERED_DN_BOARD_DN;
			break;
		case bc_audioboard:
			break;
		case bc_wallclock:
			displayConfiguration[0] = BRT_WALLCLOCK;
			break;
		case bc_chaseclock:
			displayConfiguration[0] = BRT_CHASECLOCK;
			break;
	}
#endif

	init_pins();
	init_f_cpu();
}


void hardware_assert(uint16_t line)
{
#if ASSERT_TO_BOARD
	board_debug_msg(line);
#endif

	while (1) { }
}


void debug_abuse_epb()
{
#if defined(BOARD_PCB11)
	while (TRUE)
	{
		gpio_set(BOARDSEL0);
		gpio_clr(BOARDSEL0);
		gpio_set(BOARDSEL1);
		gpio_clr(BOARDSEL1);
		gpio_set(BOARDSEL2);
		gpio_clr(BOARDSEL2);
		gpio_set(DIGSEL0);
		gpio_clr(DIGSEL0);
		gpio_set(DIGSEL1);
		gpio_clr(DIGSEL1);
		gpio_set(DIGSEL2);
		gpio_clr(DIGSEL2);
		gpio_set(SEGSEL0);
		gpio_clr(SEGSEL0);
		gpio_set(SEGSEL1);
		gpio_clr(SEGSEL1);
		gpio_set(SEGSEL2);
		gpio_clr(SEGSEL2);
		gpio_set(DATA);
		gpio_clr(DATA);
	}
#else
	assert(FALSE);
#endif // defined(BOARD_PCB11)
}

