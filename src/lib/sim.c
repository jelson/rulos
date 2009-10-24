#define DIGIT_WIDTH 5
#define DIGIT_HEIGHT 4

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <signal.h>
#include <sched.h>
#include <ctype.h>

#include "rocket.h"
#include "util.h"
#include "display_controller.h"
#include "uart.h"

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <curses.h>

#include "rocket.h"

static WINDOW *mainwnd;

struct segment_def_s {
  int xoff;
  int yoff;
  char *s;
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

  
sigset_t alrm_set;
uint32_t f_cpu = 4000000;


static void terminate_sim(void)
{
	nocbreak();
	endwin();
	exit(0);
}


void hal_upside_down_led(SSBitmap *b)
{
}

void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff)
{
	if (board < 0 || board >= NUM_BOARDS || digit < 0 || digit >= NUM_DIGITS || segment < 0 || segment >= 8)
    return;

	int x_origin = (digit) * DIGIT_WIDTH;
	int y_origin = board * DIGIT_HEIGHT;
	int i;

	x_origin += segment_defs[segment].xoff;
	y_origin += segment_defs[segment].yoff;

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

typedef void (*sim_special_input_handler_t)(char c);
typedef void (*sim_input_handler_stop_t)();

static sim_special_input_handler_t sim_special_input_handler = NULL;
static sim_input_handler_stop_t sim_input_handler_stop = NULL;


/********************** adc simulator *********************/


uint16_t adc[8];

uint16_t *hal_get_adc(uint8_t channel)
{
	adc[channel] = 1234;
	return &(adc[channel]);
}


/********************** uart simulator *********************/


static WINDOW *uart_input_window = NULL;
char recent_uart_buf[20];

static void draw_uart_input_window()
{
	mvwprintw(uart_input_window, 2, 1, "%s", recent_uart_buf);
	wrefresh(uart_input_window);
}

static void uart_simulator_input(char c)
{
	LOGF((logfp, "sim inserting to uart: %c\n", c));

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
	uart_receive(c);
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

void hal_uart_init(uint16_t baud)
{
}



/************ keypad simulator *********************/

char keypad_buf[10];
ByteQueue *keypad_q = (ByteQueue *) keypad_buf;


char hal_read_keybuf()
{
	uint8_t k;

	if (ByteQueue_pop(keypad_q, &k))
		return k;
	else
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


static void sim_poll_keyboard()
{
	int c = getch();

	if (c == ERR)
		return;

	LOGF((logfp, "poll_kb got char: %c (%x)\n", c, c));

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

	default:
		if ((k = translate_to_keybuf(c)) != 0) {
			ByteQueue_append(keypad_q, k);
		}
		break;
	}
}


/**************** clock ****************/

Handler _sensor_interrupt_handler = NULL;
Handler user_clock_handler;
const uint32_t sensor_interrupt_simulator_counter_period = 507;
uint32_t sensor_interrupt_simulator_counter;

static void sim_clock_handler()
{
	sensor_interrupt_simulator_counter += 1;
	if (sensor_interrupt_simulator_counter == sensor_interrupt_simulator_counter_period)
	{
		if (_sensor_interrupt_handler != NULL)
		{
			_sensor_interrupt_handler();
		}
		sensor_interrupt_simulator_counter = 0;
	}

	user_clock_handler();
}

void hal_start_clock_us(uint32_t us, Handler handler)
{
	struct itimerval ivalue, ovalue;
	ivalue.it_interval.tv_sec = us/1000000;
	ivalue.it_interval.tv_usec = (us%1000000);
	ivalue.it_value = ivalue.it_interval;
	setitimer(ITIMER_REAL, &ivalue, &ovalue);

	user_clock_handler = handler;
	signal(SIGALRM, sim_clock_handler);
}

// this COULD be implemented with gettimeofday(), but I'm too lazy,
// since the only reason this function exists is for the wall clock
// app, so it only matters in hardware.
uint16_t hal_elapsed_milliintervals()
{
	return 0;
}

void hal_speedup_clock_ppm(int32_t ratio)
{
	// do nothing for now
}

void hal_start_atomic()
{
	sigprocmask(SIG_BLOCK, &alrm_set, NULL);
}

void hal_end_atomic()
{
	sigprocmask(SIG_UNBLOCK, &alrm_set, NULL);
}

void hal_idle()
{
	// turns out 'man sleep' says sleep & sigalrm don't mix. yield is what we want.
	// No, sched_yield doesn't wait ANY time. libc suggests select()
	static struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100;	// .1 ms
	select(0, NULL, NULL, NULL, &tv);
	sim_poll_keyboard();
}

void hal_delay_ms(uint16_t ms)
{
	static struct timeval tv;
	tv.tv_sec = ms/1000;
	tv.tv_usec = 1000*(ms%1000);
	select(0, NULL, NULL, NULL, &tv);
}


void sensor_interrupt_register_handler(Handler handler)
{
	_sensor_interrupt_handler = handler;
}



void hal_init()
{
	/* init input buffers */
	ByteQueue_init((ByteQueue *) keypad_buf, sizeof(keypad_buf));
	memset(recent_uart_buf, 0, sizeof(recent_uart_buf));

	/* init curses */
	mainwnd = initscr();
	noecho();
	cbreak();
	nodelay(mainwnd, TRUE);
	refresh();
	curs_set(0);

	/* init screen */
	SSBitmap value = ascii_to_bitmap('8') | SSB_DECIMAL;
	int board, digit;
	for (board = 0; board < NUM_BOARDS; board++) {
		for (digit = 0; digit < NUM_DIGITS; digit++) {
			program_cell(board, digit, value);
		}
	}

	/* init clock stuff */
	sigemptyset(&alrm_set);
	sigaddset(&alrm_set, SIGALRM);
}
