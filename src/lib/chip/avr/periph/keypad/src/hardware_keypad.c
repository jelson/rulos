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
 * hardware_keypad.c: Functions for scanning a hardware matrix keypad
 *
 * This file is not compiled by the simulator.
 */

#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>

#include "rulos.h"
#include "hardware.h"
#include "board_defs.h"
#include "hal.h"


///////// HAL functions (public interface) ////////////////////////

#ifndef KEYPAD_COL0
# error "Board definitions for this board don't include KEYPAD definitions."
#else

#define KEY_SCAN_INTERVAL_US 10000
#define KEY_REFRACTORY_TIME_US 30000

typedef struct {
	char keypad_q[20];
	char keypad_last;
	Time keypad_next_allowed_key_time;
} KeypadState;

static KeypadState g_theKeypad;
static void init_keypad(KeypadState *keypad);
static void keypad_update(KeypadState *key);

void hal_init_keypad()
{
	init_keypad(&g_theKeypad);
}

char hal_read_keybuf()
{
	char k;

	if (CharQueue_pop((CharQueue *) g_theKeypad.keypad_q, &k))
		return k;
	else
		return 0;
}



/////////// Hardware Implementation /////////////////


static void init_keypad(KeypadState *keypad)
{
	gpio_make_input_enable_pullup(KEYPAD_COL0);
	gpio_make_input_enable_pullup(KEYPAD_COL1);
	gpio_make_input_enable_pullup(KEYPAD_COL2);
	gpio_make_input_enable_pullup(KEYPAD_COL3);
	// these row setups may be redundant on board defs that
	// re-use the row outputs, but some boards have keypads without LEDs,
	// and we need to set up the ports.
	gpio_make_output(KEYPAD_ROW0);
	gpio_make_output(KEYPAD_ROW1);
	gpio_make_output(KEYPAD_ROW2);
	gpio_make_output(KEYPAD_ROW3);

	CharQueue_init((CharQueue *) keypad->keypad_q, sizeof(keypad->keypad_q));
	keypad->keypad_last = 0;
	keypad->keypad_next_allowed_key_time = clock_time_us();

	schedule_us(1, (ActivationFuncPtr) keypad_update, keypad);
}

static void keypad_update(KeypadState *key)
{
	schedule_us(KEY_SCAN_INTERVAL_US, (ActivationFuncPtr) keypad_update, key);

	char k = hal_scan_keypad();

	// if the same key is down, or up, ignore it
	if (k == key->keypad_last)
		return;

	key->keypad_last = k;

	// key was just released.  Set refactory time.
	if (k == 0) {
		key->keypad_next_allowed_key_time = clock_time_us() + KEY_REFRACTORY_TIME_US;
		return;
	}

	// key was just pushed.  if refrac time has arrived, queue it
	if (clock_time_us() >= key->keypad_next_allowed_key_time) {
		CharQueue_append((CharQueue *)key->keypad_q, k);
	}
	
}

static uint8_t scan_row()
{
	/*
	 * Scan the four columns in of the row.  Return 1..4 if any of
	 * them are low.  Return 0 if they're all high.
	 */
	_NOP();
	if (gpio_is_clr(KEYPAD_COL0)) return 1;
	if (gpio_is_clr(KEYPAD_COL1)) return 2;
	if (gpio_is_clr(KEYPAD_COL2)) return 3;
	if (gpio_is_clr(KEYPAD_COL3)) return 4;

	return 0;
}


char hal_scan_keypad()
{
	uint8_t col;

	/* Scan first row */
	gpio_clr(KEYPAD_ROW0);
	gpio_set(KEYPAD_ROW1);
	gpio_set(KEYPAD_ROW2);
	gpio_set(KEYPAD_ROW3);
	col = scan_row();
	if (col == 1)			return '1';
	if (col == 2)			return '2';
	if (col == 3)			return '3';
	if (col == 4)			return 'a';

	/* Scan second row */
	gpio_set(KEYPAD_ROW0);
	gpio_clr(KEYPAD_ROW1);
	gpio_set(KEYPAD_ROW2);
	gpio_set(KEYPAD_ROW3);
	col = scan_row();
	if (col == 1)			return '4';
	if (col == 2)			return '5';
	if (col == 3)			return '6';
	if (col == 4)			return 'b';

	/* Scan third row */
	gpio_set(KEYPAD_ROW0);
	gpio_set(KEYPAD_ROW1);
	gpio_clr(KEYPAD_ROW2);
	gpio_set(KEYPAD_ROW3);
	col = scan_row();
	if (col == 1)			return '7';
	if (col == 2)			return '8';
	if (col == 3)			return '9';
	if (col == 4)			return 'c';

	/* Scan fourth row */
	gpio_set(KEYPAD_ROW0);
	gpio_set(KEYPAD_ROW1);
	gpio_set(KEYPAD_ROW2);
	gpio_clr(KEYPAD_ROW3);
	col = scan_row();
	if (col == 1)			return 's';
	if (col == 2)			return '0';
	if (col == 3)			return 'p';
	if (col == 4)			return 'd';

	return 0;
}

#endif  // KEYPAD_COL0
