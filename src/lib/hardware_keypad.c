/*
 * hardware_keypad.c: Functions for scanning a hardware matrix keypad
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


///////// HAL functions (public interface) ////////////////////////

#define KEY_SCAN_INTERVAL_US 10000
#define KEY_REFRACTORY_TIME_US 30000

typedef struct {
	ActivationFunc func;
	char keypad_q[20];
	char keypad_last;
	Time keypad_next_allowed_key_time;
} KeypadState;

static KeypadState g_theKeypad;
static void init_keypad(KeypadState *keypad);
static void keypad_update(KeypadState *key);
static char scan_keypad();

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
	keypad->func = (ActivationFunc) keypad_update;
	CharQueue_init((CharQueue *) keypad->keypad_q, sizeof(keypad->keypad_q));
	keypad->keypad_last = 0;
	keypad->keypad_next_allowed_key_time = clock_time_us();

	schedule_us(1, (Activation *) keypad);
}

static void keypad_update(KeypadState *key)
{
	schedule_us(KEY_SCAN_INTERVAL_US, (Activation *) key);

	char k = scan_keypad();

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


#ifdef KEYPAD_ROW0

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
#endif


static char scan_keypad()
{
#ifdef KEYPAD_ROW0
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
#endif

	return 0;
}

