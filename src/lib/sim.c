#define DIGIT_WIDTH 5
#define DIGIT_HEIGHT 4

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <signal.h>
#include <sched.h>
#include <ctype.h>
#include <curses.h>
#include <fcntl.h>

// TWI simulator
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#include "rocket.h"
#include "util.h"
#include "display_controller.h"
#include "uart.h"
#include "sim.h"

void twi_poll(const char *source);
void sim_audio_poll(const char *source);
r_bool sim_audio_poll_once();
void sim_twi_set_instance(int instance);

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

  
sigset_t mask_set;
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

#define PAIR_GREEN	1
#define PAIR_YELLOW	2
#define PAIR_RED	3
#define PAIR_BLUE	4
#define PAIR_WHITE	5
#define PG PAIR_GREEN
#define PY PAIR_YELLOW
#define PR PAIR_RED
#define PB PAIR_BLUE
#define PW PAIR_WHITE

BoardLayout tree0_def[] = {
	{ "Mission Clock",			{ PG,PG,PG,PG,PG,PG,PY,PY },  3,  0 },
	{ "Distance to Moon",		{ PR,PR,PR,PR,PR,PR,PR,PR }, 33,  4 },
	{ "Speed (fps)",			{ PY,PY,PY,PY,PY,PY,PY,PY }, 33,  8 },
	{ "Thruster Actuation",		{ PB,PR,PR,PR,PB,PY,PY,PY },  3, 12 },
	{ "",						{ PR,PR,PR,PR,PR,PR,PR,PR },  9, 16 },
	{ "",						{ PR,PR,PR,PR,PR,PR,PR,PR },  9, 19 },
	{ "",						{ PR,PR,PR,PR,PR,PR,PR,PR },  9, 22 },
	{ "",						{ PR,PR,PR,PR,PR,PR,PR,PR },  9, 25 },
	{ NULL }
}, *tree0 = tree0_def;

BoardLayout tree1_def[] = {
	{ "Azimuth, Elevation",		{ PG,PG,PG,PR,PR,PY,PY,PY }, 33, 0 },
	{ "Liquid Hydrogen Prs",	{ PG,PG,PG,PG,PG,PG,PG,PG },  3, 4 },
	{ "Flight Computer",		{ PR,PR,PR,PR,PY,PY,PY,PY }, 17, 9 },
	{ "",						{ PG,PG,PG,PG,PB,PB,PB,PB }, 17, 12 },
	{ NULL },
	{ NULL },
	{ NULL },
	{ NULL }
}, *tree1 = tree1_def;

BoardLayout tree2_def[] = {
	{ "AudioBoard",		{ PG,PG,PG,PG,PY,PY,PY,PY }, 1, 0 },
	{ NULL },
	{ NULL },
	{ NULL },
	{ NULL },
	{ NULL },
	{ NULL },
	{ NULL }
}, *tree2 = tree2_def;

BoardLayout wallclock_tree_def[] = {
	{ "Clock",		{ PG,PG,PG,PG,PG,PG,PB,PB }, 15, 0 },
	{ NULL },
	{ NULL },
	{ NULL },
	{ NULL },
	{ NULL },
	{ NULL },
	{ NULL }
}, *wallclock_tree = wallclock_tree_def;

BoardLayout *g_sim_theTree = NULL;

void sim_configure_tree(BoardLayout *tree)
{
	g_sim_theTree = tree;
}

void hal_program_labels()
{
	BoardLayout *bl;
	assert(g_sim_theTree!=NULL); // You forgot to call sim_configure_tree.
	for (bl = g_sim_theTree; bl->label != NULL; bl+=1)
	{
		attroff(A_BOLD);
		wcolor_set(mainwnd, PW, NULL);
		mvwprintw(mainwnd,
			bl->y, bl->x+(4*NUM_DIGITS-strlen(bl->label))/2, bl->label);
	}
}

void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff)
{
	if (board < 0 || board >= NUM_BOARDS || digit < 0 || digit >= NUM_DIGITS || segment < 0 || segment >= 8 || g_sim_theTree[board].label==NULL)
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



/********************** uart simulator *********************/


static WINDOW *uart_input_window = NULL;
char recent_uart_buf[20];

static void draw_uart_input_window()
{
	mvwprintw(uart_input_window, 2, 1, "%s", recent_uart_buf);
	wrefresh(uart_input_window);
}

static void uart_simulator_input(int c)
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

	case 'i':
		adc_simulator_start();
		break;

	case '!':	// center ADC
		adc[2] =  512;	//y
		adc[3] =  512;	//x
		break;
	case '@':	// back-left
		adc[2] =    0;	//y
		adc[3] =    0;	//x
		break;
	case '#':	// fwd
		adc[2] = 1023;	//y
		adc[3] =  512;	//x
		break;
	case '$':	// back-right
		adc[2] =    0;	//y
		adc[3] = 1023;	//x
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

	twi_poll("clock poll");
	sim_audio_poll("clock poll");

	user_clock_handler();
}

uint32_t hal_start_clock_us(uint32_t us, Handler handler, uint8_t timer_id)
{
	struct itimerval ivalue, ovalue;
	ivalue.it_interval.tv_sec = us/1000000;
	ivalue.it_interval.tv_usec = (us%1000000);
	ivalue.it_value = ivalue.it_interval;
	setitimer(ITIMER_REAL, &ivalue, &ovalue);

	user_clock_handler = handler;
	signal(SIGALRM, sim_clock_handler);
	
	return us;
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
	sigprocmask(SIG_BLOCK, &mask_set, NULL);
}

void hal_end_atomic()
{
	sigprocmask(SIG_UNBLOCK, &mask_set, NULL);
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

void hal_init(BoardConfiguration bc)
{
	switch (bc)
	{
		case bc_rocket0:
			sim_configure_tree(tree0);
			sim_twi_set_instance(0);
			break;
		case bc_rocket1:
			sim_configure_tree(tree1);
			sim_twi_set_instance(1);
			break;
		case bc_audioboard:
			sim_configure_tree(tree2);
			sim_twi_set_instance(2);
			break;
		case bc_wallclock:
			sim_configure_tree(wallclock_tree);
			sim_twi_set_instance(3);
			break;
		default:
			assert(FALSE);	// configuration not defined in simulator
	}

	/* init input buffers */
	ByteQueue_init((ByteQueue *) keypad_buf, sizeof(keypad_buf));
	memset(recent_uart_buf, 0, sizeof(recent_uart_buf));

	/* init curses */
	mainwnd = initscr();
	start_color();
	init_pair(PB, COLOR_BLUE, COLOR_BLACK);
	init_pair(PY, COLOR_YELLOW, COLOR_BLACK);
	init_pair(PG, COLOR_GREEN, COLOR_BLACK);
	init_pair(PR, COLOR_RED, COLOR_BLACK);
	init_pair(PW, COLOR_WHITE, COLOR_BLACK);
	//attron(COLOR_PAIR(1));
	noecho();
	cbreak();
	nodelay(mainwnd, TRUE);
	refresh();
	curs_set(0);

	/* init screen */
	hal_program_labels();
	SSBitmap value = ascii_to_bitmap('8') | SSB_DECIMAL;
	int board, digit;
	for (board = 0; board < NUM_BOARDS; board++) {
		for (digit = 0; digit < NUM_DIGITS; digit++) {
			program_cell(board, digit, value);
		}
	}

	/* init clock stuff */
	sigemptyset(&mask_set);
	sigaddset(&mask_set, SIGALRM);
	sigaddset(&mask_set, SIGIO);
}

TWIState g_sim_twi_state = {FALSE};

void sim_twi_set_instance(int instance)
{
	assert(0<=instance && instance<SIM_TWI_NUM_NODES);
	g_sim_twi_state.instance = instance;
}

void sim_sigio_handler()
{
	twi_poll("sigio");
	sim_audio_poll("sigio");
}

void hal_twi_init(Activation *act)
{
	signal(SIGIO, sim_sigio_handler);

	g_sim_twi_state.user_act = act;
	g_sim_twi_state.udp_socket = socket(PF_INET, SOCK_DGRAM, 0);
	g_sim_twi_state.read_ready = FALSE;

	int rc;
	int on = 1;
	int flags = fcntl(g_sim_twi_state.udp_socket, F_GETFL);
	rc = fcntl(g_sim_twi_state.udp_socket, F_SETFL, flags | O_ASYNC);
	assert(rc==0);
	rc = ioctl(g_sim_twi_state.udp_socket, FIOASYNC, &on );
	assert(rc==0);
	rc = ioctl(g_sim_twi_state.udp_socket, FIONBIO, &on);
	assert(rc==0);
	int flag = -getpid();
	rc = ioctl(g_sim_twi_state.udp_socket, SIOCSPGRP, &flag);
	assert(rc==0);

	struct sockaddr_in sai;
	sai.sin_family = AF_INET;
	sai.sin_addr.s_addr = htonl(INADDR_ANY);
	sai.sin_port = htons(SIM_TWI_PORT_BASE + g_sim_twi_state.instance);
	bind(g_sim_twi_state.udp_socket, (struct sockaddr*)&sai, sizeof(sai));
	g_sim_twi_state.initted = TRUE;
}

void twi_poll(const char *source)
{
	if (!g_sim_twi_state.initted)
	{
		return;
	}
	if (g_sim_twi_state.read_ready)
	{
		// best not consume any more packets until we've delivered this one.
		return;
	}
	
	char buf[10];
	int rc = recv(g_sim_twi_state.udp_socket, buf, sizeof(buf), MSG_DONTWAIT);
	if (rc<0)
	{
		assert(errno == EAGAIN);
		return;
	}
	if (rc>1)
	{
		LOGF((logfp, "Discarding %d-byte suffix of long packet\n", rc-1));
	}

	assert(rc>0);
	//LOGF((logfp, "sim reads TWI byte %02x; ignores %d more\n", buf[0], rc-1));

	g_sim_twi_state.read_byte = buf[0];
	g_sim_twi_state.read_ready = TRUE;
	//LOGF((logfp, "scheduled net_update from %s\n", source));
	schedule_now(g_sim_twi_state.user_act);
}

r_bool hal_twi_ready_to_send()
{
	return TRUE;
}

void hal_twi_send_byte(uint8_t byte)
{
/*
	LOGF((logfp, "hal_twi_send_byte(%02x [%c])\n",
		byte,
		(byte>=' ' && byte<127) ? byte : '_'));
*/
	
	struct sockaddr_in sai;
	int i;
	sai.sin_family = AF_INET;
	sai.sin_addr.s_addr = htonl(0x7f000001);
	for (i=0; i<SIM_TWI_NUM_NODES; i++)
	{
		// I ignore the bytes I sent on the TWI. I hope that's
		// a correct model, or the real network code gets much tricksier!
		if (i==g_sim_twi_state.instance)
		{
			continue;
		}
		sai.sin_port = htons(SIM_TWI_PORT_BASE + i);
		sendto(g_sim_twi_state.udp_socket, &byte, 1,
			0, (struct sockaddr*)&sai, sizeof(sai));
	}

	//LOGF((logfp, "scheduled net_update from %s\n", "hal_twi_send_byte"));
	schedule_now(g_sim_twi_state.user_act);
}

r_bool hal_twi_read_byte(/*OUT*/ uint8_t *byte)
{
	if (g_sim_twi_state.read_ready)
	{
		*byte = g_sim_twi_state.read_byte;
		g_sim_twi_state.read_ready = FALSE;
		// whoah, another byte is ready RIGHT NOW!
		//LOGF((logfp, "scheduled net_update from %s\n", "hal_twi_read_byte"));
		schedule_now(g_sim_twi_state.user_act);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

#define SIM_AUDIO_BUF_SIZE 1000
typedef struct {
	uint8_t pcm[SIM_AUDIO_BUF_SIZE];
	uint16_t ptr;
	uint16_t size;
} SimAudioBuf;

typedef struct s_SimAudioState {
	int devdspfd;
	SimAudioBuf bufs[2];
	SimAudioBuf *cur;
	SimAudioBuf *next;
	HalAudioRefillIfc *refill;
} SimAudioState;
SimAudioState alloc_simAudioState, *simAudioState = NULL;

void setasync(int fd)
{
	int flags, rc;
	flags = fcntl(fd, F_GETFL);
	rc = fcntl(fd, F_SETFL, flags | O_ASYNC | O_NONBLOCK);
	assert(rc==0);
}

void hal_audio_init(uint16_t sample_period_us, HalAudioRefillIfc *refill)
{
	simAudioState = &alloc_simAudioState;
	simAudioState->devdspfd = open("/dev/dsp", O_ASYNC|O_WRONLY);
	//simAudioState->devdspfd = open("devdsp", O_CREAT|O_WRONLY);
	simAudioState->cur = &simAudioState->bufs[0];
	simAudioState->next = &simAudioState->bufs[0];
	simAudioState->cur->size = 0;
	simAudioState->cur->ptr = 0;
	simAudioState->refill = refill;
	simAudioState->next->size = 0;	// mark next as ready to refill

	assert(simAudioState->devdspfd>0);
	setasync(simAudioState->devdspfd);
}

void sim_audio_poll(const char *source)
{
	while (TRUE)
	{
		if (sim_audio_poll_once())
		{
			break;
		}
	}
}

r_bool sim_audio_poll_once()
{
	if (simAudioState==NULL)
	{
		// Not initted.
		return TRUE;
	}

	// try to write some stuff.
	// wantWrite == 0 if both buffers ever get fully drained.
	// size/4-> give us extra chances to refill next buf
	int wantWrite = min(simAudioState->cur->size - simAudioState->cur->ptr, SIM_AUDIO_BUF_SIZE/4);
	int wrote = 0;
	r_bool filled_devdsp = FALSE;
	if (wantWrite>0)
	{
		wrote = write(
			simAudioState->devdspfd,
			simAudioState->cur->pcm+simAudioState->cur->ptr,
			wantWrite);
		if (wrote<0)
		{
			assert(errno==EAGAIN);
			return TRUE;
		}
		LOGF((logfp, "wrote %d/%d bytes to devdsp\n", wrote, wantWrite));
		filled_devdsp = (wrote < wantWrite);
	}
	else
	{
		// hmm -- didn't fill DSP, but don't have anything to say and
		// hence don't want to loop.
		filled_devdsp = FALSE;
		LOGF((logfp, "DSP is ahead of me.\n"));
	}
	
	simAudioState->cur->ptr += wrote;

	// if we emptied a buffer, refill
	assert(simAudioState->cur->ptr <= simAudioState->cur->size);
	if (simAudioState->cur->ptr == simAudioState->cur->size)
	{
		// swap buffers
		SimAudioBuf *tmp = simAudioState->cur;
		simAudioState->cur = simAudioState->next;
		simAudioState->next = tmp;

		// put next buf in a safe state in case we swap back to it.
		simAudioState->next->size = 0;
		simAudioState->next->ptr = 0;
	}

	// always see if we can keep filling the next buffer
	if (SIM_AUDIO_BUF_SIZE - simAudioState->next->size > 0)
	//if (simAudioState->next->size == 0)
	{
		// try to refill next buffer
		uint16_t size = simAudioState->refill->func(
			simAudioState->refill,
			&simAudioState->next->pcm[simAudioState->next->size],
			SIM_AUDIO_BUF_SIZE - simAudioState->next->size);
		simAudioState->next->size += size;
	}

	return filled_devdsp;
}

typedef enum {
	sss_ready,
	sss_read_addr,
	sss_read_fetch,
} SimSpiState;

typedef struct s_sim_spi {
	HALSPIIfc *spi_ifc;
	FILE *fp;
	SimSpiState state;
	int addr_off;
	uint8_t addr[3];
} SimSpi;
SimSpi g_spi;

void hal_init_spi(HALSPIIfc *spi)
{
	g_spi.spi_ifc = spi;
	g_spi.fp = fopen("spiflash.bin", "r");
	assert(g_spi.fp != NULL);
}

void hal_spi_open()
{
	g_spi.state = sss_ready;
}

void hal_spi_send(uint8_t byte)
{
	uint8_t result = 0;
	switch (g_spi.state)
	{
		case sss_ready:
			if (byte==SPIFLASH_CMD_READ_DATA)
			{
				g_spi.state = sss_read_addr;
				g_spi.addr_off = 0;
			}
			else
			{
				assert(FALSE); // "unsupported SPI command"
			}
			break;
		case sss_read_addr:
			g_spi.addr[g_spi.addr_off++] = byte;
			if (g_spi.addr_off == 3)
			{
				int addr =
					  (((int)g_spi.addr[0])<<16)
					| (((int)g_spi.addr[1])<<8)
					| (((int)g_spi.addr[2])<<0);
				fseek(g_spi.fp, addr, SEEK_SET);
				g_spi.state = sss_read_fetch;
			}
			break;
		case sss_read_fetch:
			result = fgetc(g_spi.fp);
			break;	
	}

	// deliver on stack. This may be a little infinite-recursion-y
	g_spi.spi_ifc->func(g_spi.spi_ifc, result);
}

void hal_spi_close()
{
	g_spi.state = sss_ready;
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

