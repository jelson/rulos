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

#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "chip/sim/core/sim.h"
#include "core/util.h"
#include "periph/7seg_panel/board_config.h"
#include "periph/7seg_panel/display_controller.h"
#include "periph/ring_buffer/rocket_ring_buffer.h"
#include "periph/rocket/rocket.h"
#include "periph/uart/uart.h"

r_bool g_joystick_trigger_state;


/*
   __
  |__|
  |__|.

seg  x  y char
0    1  0 __
1    3  1 |
2    3  2 |
3    1  2 __
4    0  2 |
5    0  1 |
6    1  1 __
7    4  2 .

*/

#define DIGIT_WIDTH 5
#define DIGIT_HEIGHT 4

static WINDOW *mainwnd;

struct segment_def_s {
  int xoff;
  int yoff;
  const char *s;
} segment_defs[] =  {
  { 1, 0, "__" },
  { 3, 1, "|" },
  { 3, 2, "|" },
  { 1, 2, "__" },
  { 0, 2, "|" },
  { 0, 1, "|" },
  { 1, 1, "__" },
  { 4, 2, "." }
};

  
static void terminate_sim(void)
{
	nocbreak();
	endwin();
	exit(0);
}


void hal_upside_down_led(SSBitmap *b)
{
}

#define PAIR_GREEN	1
#define PAIR_YELLOW	2
#define PAIR_RED	3
#define PAIR_BLUE	4
#define PAIR_WHITE	5
#define PAIR_BLACK_ON_WHITE	6
#define PG PAIR_GREEN,
#define PY PAIR_YELLOW,
#define PR PAIR_RED,
#define PB PAIR_BLUE,
#define PW PAIR_WHITE,

#define	DBOARD(name, syms, x, y) \
	{ name, {syms}, x, y }
#define B_NO_BOARD	{ NULL },
#define B_END	{ NULL }
#include "core/board_defs.h"

BoardLayout tree0_def[] = { T_ROCKET0 }, *tree0 = tree0_def;

BoardLayout tree1_def[] = { T_ROCKET1 }, *tree1 = tree1_def;

BoardLayout wallclock_tree_def[] = {
	{ "Clock",		{ PG PG PG PG PG PG PB PB }, 15, 0 },
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_END
}, *wallclock_tree = wallclock_tree_def;

BoardLayout chaseclock_tree_def[] = {
	{ "Clock",		{ PG PG PG PG PY PY PY PY }, 15, 0 },
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_END
}, *chaseclock_tree = chaseclock_tree_def;

BoardLayout default_tree_def[] = {
	{ "board0", {PG PG PG PG PG PG PG PG }, 15, 0 },
	{ "board1", {PG PG PG PG PG PG PG PG }, 15, 4 },
	{ "board2", {PG PG PG PG PG PG PG PG }, 15, 8 },
	{ "board3", {PG PG PG PG PG PG PG PG }, 15, 12 },
	{ "board4", {PG PG PG PG PG PG PG PG }, 15, 16 },
	{ "board5", {PG PG PG PG PG PG PG PG }, 15, 20 },
	{ "board6", {PG PG PG PG PG PG PG PG }, 15, 24 },
	{ "board7", {PG PG PG PG PG PG PG PG }, 15, 28 },
	B_END
}, *default_tree = default_tree_def;

BoardLayout *g_sim_theTree = NULL;

void sim_configure_tree(BoardLayout *tree)
{
	g_sim_theTree = tree;
}

static void sim_program_labels()
{
	BoardLayout *bl;
	assert(g_sim_theTree!=NULL); // You forgot to call sim_configure_tree.
	for (bl = g_sim_theTree; bl->label != NULL; bl+=1)
	{
		attroff(A_BOLD);
		wcolor_set(mainwnd, PAIR_WHITE, NULL);
		mvwprintw(mainwnd,
			bl->y, bl->x+(4*NUM_DIGITS-strlen(bl->label))/2, bl->label);
	}
}

void sim_display_light_status(r_bool status)
{
	if (status)
	{
		wcolor_set(mainwnd, PAIR_BLACK_ON_WHITE, NULL);
		mvwprintw(mainwnd, 17, 50, " Lights on  ");
	}
	else
	{
		wcolor_set(mainwnd, PAIR_WHITE, NULL);
		mvwprintw(mainwnd, 17, 50, " Lights off ");
	}
}

void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff)
{
	if (board < 0 || board >= NUM_LOCAL_BOARDS || digit < 0 || digit >= NUM_DIGITS || segment < 0 || segment >= 8 || g_sim_theTree[board].label==NULL)
    return;

	int x_origin = g_sim_theTree[board].x + (digit) * DIGIT_WIDTH;
	int y_origin = g_sim_theTree[board].y + 1;
	int i;

	x_origin += segment_defs[segment].xoff;
	y_origin += segment_defs[segment].yoff;

	attron(A_BOLD);
	wcolor_set(mainwnd, g_sim_theTree[board].colors[digit], NULL);
	if (onoff)
	{
		mvwprintw(mainwnd, y_origin, x_origin, segment_defs[segment].s);
	}
	else
	{
		for (i = strlen(segment_defs[segment].s); i; i--)
			mvwprintw(mainwnd, y_origin, x_origin+i-1, " ");
	}
    
	wrefresh(mainwnd);
}


/************** simulator input *****************/

typedef void (*sim_special_input_handler_t)(int c);
typedef void (*sim_input_handler_stop_t)();

static sim_special_input_handler_t sim_special_input_handler = NULL;
static sim_input_handler_stop_t sim_input_handler_stop = NULL;


/********************** adc simulator *********************/

#define NUM_ADC 6

static WINDOW *adc_input_window = NULL;
static int adc_input_channel = 0;
uint16_t adc[NUM_ADC];

static void draw_adc_input_window()
{
	mvwprintw(adc_input_window, 1, 1, "ADC Channel %d:", adc_input_channel);
	mvwprintw(adc_input_window, 2, 1, "%4d", adc[adc_input_channel]);
	keypad(mainwnd, 1);
	wrefresh(adc_input_window);
}

static void adc_simulator_input(int c)
{
	switch (c) {
	case KEY_RIGHT:
		adc_input_channel++;
		break;
	case KEY_LEFT:
		adc_input_channel--;
		break;
	case KEY_UP:
		adc[adc_input_channel] += 5;
		break;
	case KEY_DOWN:
		adc[adc_input_channel] -= 5;
		break;
	case KEY_PPAGE:
		adc[adc_input_channel] += 100;
		break;
	case KEY_NPAGE:
		adc[adc_input_channel] -= 100;
		break;
	}
	
	adc_input_channel = max(0, min(NUM_ADC, adc_input_channel));
	adc[adc_input_channel] = max(0, min(1023, adc[adc_input_channel]));

	draw_adc_input_window();
}


static void adc_simulator_stop()
{
	keypad(mainwnd, 0);
	sim_special_input_handler = NULL;
	sim_input_handler_stop = NULL;
	delwin(adc_input_window);
	touchwin(mainwnd);
	refresh();
}

static void adc_simulator_start()
{
	// set up handlers
	sim_special_input_handler = adc_simulator_input;
	sim_input_handler_stop = adc_simulator_stop;

	// create input window
	adc_input_window =
		newwin(4,
			   17,
			   2,
			   NUM_DIGITS*DIGIT_WIDTH + 3);
	box(adc_input_window, ACS_VLINE, ACS_HLINE);
	draw_adc_input_window();
}

void hal_init_adc(Time scan_period)
{
}

void hal_init_adc_channel(uint8_t idx)
{
	adc[idx] = 512;
}

uint16_t hal_read_adc(uint8_t idx)
{
	return adc[idx];
}

/********** uart input simulator ***************/

extern UartHandler *g_sim_uart_handler;
static WINDOW *uart_input_window = NULL;
char recent_uart_buf[20];

static void draw_uart_input_window()
{
	mvwprintw(uart_input_window, 2, 1, "%s", recent_uart_buf);
	wrefresh(uart_input_window);
}

static void uart_simulator_input(int c)
{
	LOG("sim inserting to uart: %c\n", c);

	// display on screen
	if (strlen(recent_uart_buf) == sizeof(recent_uart_buf)-1)
	{
		memmove(&recent_uart_buf[0],
				&recent_uart_buf[1],
				sizeof(recent_uart_buf)-1);
	}
	if (isprint(c))
		sprintf(recent_uart_buf+strlen(recent_uart_buf), "%c", c);
	else
		strcat(recent_uart_buf, ".");

	draw_uart_input_window();

	// upcall to the uart code
	if (g_sim_uart_handler == NULL) {
		LOG("dropping uart char - uart not initted\n");
		return;
	}
	(g_sim_uart_handler->recv)(g_sim_uart_handler, c);
}

static void uart_simulator_stop()
{
	sim_special_input_handler = NULL;
	sim_input_handler_stop = NULL;
	delwin(uart_input_window);
	touchwin(mainwnd);
	refresh();
}

static void uart_simulator_start()
{
	// set up handlers
	sim_special_input_handler = uart_simulator_input;
	sim_input_handler_stop = uart_simulator_stop;

	// create input window
	uart_input_window =
		newwin(4,
			   sizeof(recent_uart_buf)+1,
			   2,
			   NUM_DIGITS*DIGIT_WIDTH + 3);
	mvwprintw(uart_input_window, 1, 1, "UART Input Mode:");
	box(uart_input_window, ACS_VLINE, ACS_HLINE);
	draw_uart_input_window();
}

/************ keypad simulator *********************/

r_bool g_keypad_enabled = FALSE;
char keypad_buf[10];
CharQueue *keypad_q = (CharQueue *) keypad_buf;

void hal_init_keypad()
{
	g_keypad_enabled = TRUE;
}

char hal_read_keybuf()
{
	if (!g_keypad_enabled) { return 0; }

	char k;

	if (CharQueue_pop(keypad_q, &k))
		return k;
	else
		return 0;

}

char hal_scan_keypad()
{
	return 0;
}

// translation from a key typed at the keyboard to the simulated
// keypad input that should be enqueued
char translate_to_keybuf(char c)
{
	if (c >= 'a' && c <= 'd')
		return c;
	if (c == '\t') return 'a';
	if (c == '\n') return 'c';
	if (c == 27) return 'd';

	if (c >= '0' && c <= '9')
		return c;

	if (c == '*' || c == 's' || c == '.')
		return 's';

	if (c == 'p' || c == '#')
		return 'p';

	return 0;
}


static void sim_curses_poll(void *data)
{
	int c = getch();

	if (c == ERR)
		return;

	LOG("poll_kb got char: %c (%x)\n", c, c);

	// if we're in normal mode and hit 'q', terminate the simulator
	if (sim_special_input_handler == NULL && c == 'q')
	{
		terminate_sim();
	}

	// If we're in a special input mode and hit escape, exit the mode.
	// Otherwise, pass the character to the handler.
	if (sim_special_input_handler != NULL)
	{
		if (c == 27 /* escape */)
		{
			sim_input_handler_stop();
		}
		else
		{
			sim_special_input_handler(c);
		}
		return;
	}


	// Check for one of the characters that puts us into a special
	// input mode.  If none of them match, default to keypad input
	// simulation.
	char k;
	switch (c) {
	case 'u':
		uart_simulator_start();
		break;

	case 'i':
		adc_simulator_start();
		break;

	case '!':	// center ADC
		adc[2] =  512;	//y
		adc[3] =  512;	//x
		g_joystick_trigger_state = FALSE;
		break;
	case '@':	// back-left
		adc[2] =   11;	//y
		adc[3] =   11;	//x
		break;
	case '#':	// fwd
		adc[2] = 1023;	//y
		adc[3] =  512;	//x
		break;
	case '$':	// back-right
		adc[2] =   11;	//y
		adc[3] = 1023;	//x
		break;
	case '%':	// button-on
		g_joystick_trigger_state = TRUE;
		break;

	default:
		if ((k = translate_to_keybuf(c)) != 0) {
			CharQueue_append(keypad_q, k);
		}
		break;
	}
}



void hal_init_rocketpanel()
{
	hal_init();

#if defined (BOARDCONFIG_ROCKET0)
	sim_configure_tree(tree0);
#elif defined (BOARDCONFIG_ROCKET1)
	sim_configure_tree(tree1);
#elif defined (BOARDCONFIG_WALLCLOCK)
	sim_configure_tree(wallclock_tree);
#elif defined (BOARDCONFIG_CHASECLOCK)
	sim_configure_tree(chaseclock_tree);
#elif defined (BOARDCONFIG_DEFAULT) || defined(BOARDCONFIG_NETROCKET)
	sim_configure_tree(default_tree);
#else
# error "Board config not defined for rocketpanel"
#endif

	/* init input buffers */
	CharQueue_init((CharQueue *) keypad_buf, sizeof(keypad_buf));
	memset(recent_uart_buf, 0, sizeof(recent_uart_buf));

	/* init curses */
	mainwnd = initscr();
	start_color();
	init_pair(PAIR_BLUE, COLOR_BLUE, COLOR_BLACK);
	init_pair(PAIR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
	init_pair(PAIR_GREEN, COLOR_GREEN, COLOR_BLACK);
	init_pair(PAIR_RED, COLOR_RED, COLOR_BLACK);
	init_pair(PAIR_WHITE, COLOR_WHITE, COLOR_BLACK);
	init_pair(PAIR_BLACK_ON_WHITE, COLOR_BLACK, COLOR_WHITE);
	//attron(COLOR_PAIR(1));
	noecho();
	cbreak();
	nodelay(mainwnd, TRUE);
	refresh();
	curs_set(0);

	/* init screen */
	sim_program_labels();
	SSBitmap value = ascii_to_bitmap('8') | SSB_DECIMAL;
	int board, digit;
	for (board = 0; board < NUM_LOCAL_BOARDS; board++) {
		for (digit = 0; digit < NUM_DIGITS; digit++) {
			program_cell(board, digit, value);
		}
	}

	sim_register_clock_handler(sim_curses_poll, NULL);
}


/******************* joystick *****************/

void hal_init_joystick_button()
{
	g_joystick_trigger_state = FALSE;
}

r_bool hal_read_joystick_button()
{
	return g_joystick_trigger_state;
}




