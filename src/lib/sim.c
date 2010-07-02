#define DIGIT_WIDTH 5
#define DIGIT_HEIGHT 4

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
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

static void twi_poll(const char *source);
void sim_audio_poll(const char *source);
r_bool sim_audio_poll_once();
void sim_spi_poke();
r_bool g_joystick_trigger_state;

void start_audio_fork_shuttling_child();
void audio_shuttling_child(int audiofd, int flowfd);

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
#include "board_defs.h"

BoardLayout tree0_def[] = { T_ROCKET0 }, *tree0 = tree0_def;

BoardLayout tree1_def[] = { T_ROCKET1 }, *tree1 = tree1_def;

BoardLayout tree2_def[] = {
	{ "AudioBoard",		{ PG PG PG PG PY PY PY PY }, 1, 0 },
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_NO_BOARD
	B_END
}, *tree2 = tree2_def;

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
	_uart_receive(RULOS_UART0, c);
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

void hal_uart_start_send(UartState_t *u)
{
	char buf[256];
	int i = 0;

	while (_uart_get_next_character(u, &buf[i]))
		i++;

	buf[i+1] = '\0';

	LOGF((logfp, "Sent to uart: '%s'\n", buf));
}

void hal_uart_init(UartState_t *u, uint16_t baud)
{
}



/************ keypad simulator *********************/

r_bool g_keypad_enabled = FALSE;
char keypad_buf[10];
ByteQueue *keypad_q = (ByteQueue *) keypad_buf;

void hal_init_keypad()
{
	g_keypad_enabled = TRUE;
}

char hal_read_keybuf()
{
	if (!g_keypad_enabled) { return 0; }

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
			ByteQueue_append(keypad_q, k);
		}
		break;
	}
}


/**************** clock ****************/

Handler _sensor_interrupt_handler = NULL;
void *_sensor_interrupt_data = NULL;
Handler user_clock_handler;
void *user_clock_data;
const uint32_t sensor_interrupt_simulator_counter_period = 507;
uint32_t sensor_interrupt_simulator_counter;

static void sim_clock_handler()
{
	sensor_interrupt_simulator_counter += 1;
	if (sensor_interrupt_simulator_counter == sensor_interrupt_simulator_counter_period)
	{
		if (_sensor_interrupt_handler != NULL)
		{
			_sensor_interrupt_handler(_sensor_interrupt_data);
		}
		sensor_interrupt_simulator_counter = 0;
	}

	twi_poll("clock poll");
	sim_audio_poll("clock poll");
	sim_spi_poke();

	user_clock_handler(user_clock_data);
}

uint32_t hal_start_clock_us(uint32_t us, Handler handler, void *data, uint8_t timer_id)
{
	struct itimerval ivalue, ovalue;
	ivalue.it_interval.tv_sec = us/1000000;
	ivalue.it_interval.tv_usec = (us%1000000);
	ivalue.it_value = ivalue.it_interval;
	setitimer(ITIMER_REAL, &ivalue, &ovalue);

	user_clock_handler = handler;
	user_clock_data = data;
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

uint8_t is_blocked = 0;

uint8_t hal_start_atomic()
{
	sigprocmask(SIG_BLOCK, &mask_set, NULL);
	uint8_t retval = is_blocked;
	is_blocked = 1;
	return retval;
}

void hal_end_atomic(uint8_t blocked)
{
	if (!blocked)
	{
		is_blocked = 0;
		sigprocmask(SIG_UNBLOCK, &mask_set, NULL);
	}
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


void sensor_interrupt_register_handler(Handler handler, void *data)
{
	_sensor_interrupt_handler = handler;
	_sensor_interrupt_data = data;
}

void hal_init(BoardConfiguration bc)
{
	switch (bc)
	{
		case bc_rocket0:
			sim_configure_tree(tree0);
			break;
		case bc_rocket1:
			sim_configure_tree(tree1);
			break;
		case bc_audioboard:
			sim_configure_tree(tree2);
			break;
		case bc_wallclock:
			sim_configure_tree(wallclock_tree);
			break;
		case bc_chaseclock:
			sim_configure_tree(chaseclock_tree);
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

struct {
	r_bool initted;
	TWIRecvSlot *trs;
	int udp_socket;
} g_sim_twi_state = { FALSE };


void sim_sigio_handler()
{
	twi_poll("sigio");
	sim_audio_poll("sigio");
}


void hal_twi_init(Addr local_addr, TWIRecvSlot *trs)
{
	signal(SIGIO, sim_sigio_handler);

	g_sim_twi_state.trs = trs;
	g_sim_twi_state.udp_socket = socket(PF_INET, SOCK_DGRAM, 0);

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
	sai.sin_port = htons(SIM_TWI_PORT_BASE + local_addr);
	bind(g_sim_twi_state.udp_socket, (struct sockaddr*)&sai, sizeof(sai));
	g_sim_twi_state.initted = TRUE;
}

typedef struct {
	ActivationFunc f;
	TWIRecvSlot *trs;
	uint8_t len;
} recvCallbackAct_t;

recvCallbackAct_t recvCallbackAct_g;

static void doRecvCallback(recvCallbackAct_t *rca)
{
	rca->trs->func(rca->trs, rca->len);
}

static void twi_poll(const char *source)
{
	if (!g_sim_twi_state.initted)
	{
		return;
	}

	char buf[4096];

	int rc = recv(
		g_sim_twi_state.udp_socket,
		buf,
		sizeof(buf),
		MSG_DONTWAIT);

	if (rc<0)
	{
		assert(errno == EAGAIN);
		return;
	}

	assert(rc != 0);

	if (g_sim_twi_state.trs->occupied)
	{
		LOGF((logfp, "TWI SIM: Packet arrived but network stack buffer busy; dropping\n"));
		return;
	}

	if (rc > g_sim_twi_state.trs->capacity)
	{
		LOGF((logfp, "TWI SIM: Discarding %d-byte packet; too long for net stack's buffer\n", rc));
		return;
	}

	g_sim_twi_state.trs->occupied = TRUE;
	memcpy(g_sim_twi_state.trs->data, buf, rc);

	recvCallbackAct_g.f = (ActivationFunc) doRecvCallback;
	recvCallbackAct_g.trs = g_sim_twi_state.trs;
	recvCallbackAct_g.len = rc;
	schedule_now((Activation *) &recvCallbackAct_g);
}


typedef struct {
	ActivationFunc f;
	TWISendDoneFunc sendDoneCB;
	void *sendDoneCBData;
} sendCallbackAct_t;

sendCallbackAct_t sendCallbackAct_g;

static void doSendCallback(sendCallbackAct_t *sca)
{
	sca->sendDoneCB(sca->sendDoneCBData);
}

void hal_twi_send(Addr dest_addr, char *data, uint8_t len,
				  TWISendDoneFunc sendDoneCB, void *sendDoneCBData)
{
/*
	LOGF((logfp, "hal_twi_send_byte(%02x [%c])\n",
		byte,
		(byte>=' ' && byte<127) ? byte : '_'));
*/
	
	struct sockaddr_in sai;

	sai.sin_family = AF_INET;
	sai.sin_addr.s_addr = htonl(0x7f000001);
	sai.sin_port = htons(SIM_TWI_PORT_BASE + dest_addr);
	sendto(g_sim_twi_state.udp_socket, data, len,
		   0, (struct sockaddr*)&sai, sizeof(sai));

	if (sendDoneCB)
	{
		sendCallbackAct_g.f = (ActivationFunc) doSendCallback;
		sendCallbackAct_g.sendDoneCB = sendDoneCB;
		sendCallbackAct_g.sendDoneCBData = sendDoneCBData;
		schedule_now((Activation *) &sendCallbackAct_g);
	}
}


#define AUDIO_BUF_SIZE 30
typedef struct s_SimAudioState {
	int audiofd;
	int flowfd;
	RingBuffer *ring;
	uint8_t _storage[sizeof(RingBuffer)+1+AUDIO_BUF_SIZE];
	int write_avail;
} SimAudioState;
SimAudioState alloc_simAudioState, *simAudioState = NULL;

void setnonblock(int fd)
{
	int flags, rc;
	flags = fcntl(fd, F_GETFL);
	rc = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	assert(rc==0);
}

RingBuffer *hal_audio_init(uint16_t sample_period_us)
{
	assert(simAudioState == NULL);	// duplicate initialization
	simAudioState = &alloc_simAudioState;
	simAudioState->ring = (RingBuffer*) simAudioState->_storage;
	init_ring_buffer(simAudioState->ring, sizeof(simAudioState->_storage));
	simAudioState->write_avail = 0;

	start_audio_fork_shuttling_child(simAudioState);

	return simAudioState->ring;
}

void start_audio_fork_shuttling_child(SimAudioState *sas)
{
	// Now, you'd think that you could just open /dev/dsp with O_NONBLOCK
	// and just write to it as it accepts bytes, right? You'd think that,
	// but then you'd be wrong, see, because it just doesn't work right;
	// it produces blips and pops that sound like gaps in the stream.
	// I tried all sorts of variations, from select(timeout=NULL), to
	// select(timeout=.5ms), to busy-waiting, but you just can't seem to
	// get the bytes in fast enough with O_NONBLOCK to keep the driver
	// full. Perhaps the audio driver has a broken implementation of
	// nonblocking-mode file descriptors? I do not know; all I know is
	// that the "obvious thing" sure isn't working.
	//
	// So my apalling (apollo-ing?) workaround: fork a child process
	// to read from a pipe and patiently feed the bytes into a nonblocking
	// fd to /dev/dsp. Yes. It works.
	//
	// Of course, pipes have something like 32k of buffer, which introduces
	// an unacceptably-long 4 sec latency in the audio stream, on top of the
	// already-too-long-but-non-negotiable .25-sec latency due to the 1k
	// buffer in /dev/dsp.
	//
	// So my even-more-apalling (gemini-ing?) workaroundaround: have the
	// child process signal flow control by sending bytes on an *upstream*
	// pipe; each upstream byte means there's room for another downstream
	// byte. Sheesh.

	int rc;
	int audiofds[2];
	rc = pipe(audiofds);
	int flowfds[2];
	rc = pipe(flowfds);

	int pid = fork();
	assert(pid>=0);

	if (pid==0)
	{
		// child
		audio_shuttling_child(audiofds[0], flowfds[1]);
		assert(FALSE); // should not return
	}

	simAudioState->audiofd = audiofds[1];
	setnonblock(simAudioState->audiofd);
	simAudioState->flowfd = flowfds[0];
	setnonblock(simAudioState->flowfd);
}

void audio_shuttling_child(int audiofd, int flowfd)
{
	char flowbuf = 0;
	char audiobuf[6];
	int rc;

	int devdspfd = open("/dev/dsp", O_WRONLY, 0);
	assert(devdspfd >= 0);
	int debugfd = open("devdsp", O_CREAT|O_WRONLY, S_IRWXU);
	assert(debugfd >= 0);

	// give buffer a little depth.
	// Smaller is better (less latency);
	// but you need a minimum amount. In audioboard, we run the
	// system clock at 1kHz (1ms). The audio subsystem is polled off
	// the system clock, so we need to be able to fill at least 8 samples
	// every poll to keep up with the buffer's drain rate.
	// Experimentation shows that 10 is the minimum value that avoids
	// clicks and pops.
	int i;
	for (i=0; i<12; i++)
	{
		rc = write(flowfd, &flowbuf, 1);
		assert(rc==1);
	}
	
	while (1)
	{
		// loop doing blocking reads and, more importantly,
		// writes, which seem to stitch acceptably on /dev/dsp
		rc = read(audiofd, audiobuf, 1);
		assert(rc==1);
		rc = write(devdspfd, audiobuf, 1);
		assert(rc==1);
		rc = write(debugfd, audiobuf, 1);
		assert(rc==1);

		// release a byte of flow control
		rc = write(flowfd, &flowbuf, 1);
		assert(rc==1);
	}
}

void debug_audio_rate()
{
	static int counter=0;
	static struct timeval start_time;
	if (counter==0)
	{
		gettimeofday(&start_time, NULL);
	}
	counter += 1;
	if (counter==900)
	{
		struct timeval end_time;
		gettimeofday(&end_time, NULL);
		suseconds_t tv_usec = end_time.tv_usec - start_time.tv_usec;
		time_t tv_sec = end_time.tv_sec - start_time.tv_sec;
		tv_usec += tv_sec*1000000;
		float rate = 900.0/tv_usec*1000000.0;
		LOGF((logfp, "output rate %f samples/sec\n", rate));
		counter = 0;
	}
}

void sim_audio_poll(const char *source)
{
	int rc;

	if (simAudioState==NULL) { return; }

	// check for flow control signal
	char flowbuf[10];
	rc = read(simAudioState->flowfd, flowbuf, sizeof(flowbuf));
	if (rc>=0)
	{
		simAudioState->write_avail += rc;
	}

	while (simAudioState->write_avail > 0
		&& ring_remove_avail(simAudioState->ring) > 0)
	{
		uint8_t sample = ring_remove(simAudioState->ring);
		//LOGF((logfp, "sim_audio_poll removes sample %2x\n", sample));
		int wrote = write(simAudioState->audiofd, &sample, 1);
		assert(wrote==1);

		simAudioState->write_avail -= 1;

		debug_audio_rate();
	}
}

typedef enum {
	sss_ready,
	sss_read_addr,
	sss_read_fetch,
} SimSpiState;

typedef struct s_sim_spi {
	r_bool initted;
	HALSPIIfc *spi_ifc;
	FILE *fp;
	SimSpiState state;
	int addr_off;
	uint8_t addr[3];
	int16_t result;
	uint8_t recursions;
} SimSpi;
SimSpi g_spi = { FALSE };

void hal_init_spi(HALSPIIfc *spi)
{
	g_spi.spi_ifc = spi;
	g_spi.fp = fopen("obj.sim/spiflash.bin", "r");
	assert(g_spi.fp != NULL);
	g_spi.initted = TRUE;
}

void hal_spi_open()
{
	g_spi.state = sss_ready;
}

void hal_spi_send(uint8_t byte)
{
	g_spi.recursions += 1;

	uint8_t result = 0;
	switch (g_spi.state)
	{
		case sss_ready:
			LOGF((logfp, "sim:spi cmd %x\n", byte));
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
			//LOGF((logfp, "sim:spi addr[%d] == %x\n", g_spi.addr_off, byte));
			g_spi.addr[g_spi.addr_off++] = byte;
			if (g_spi.addr_off == 3)
			{
				int addr =
					  (((int)g_spi.addr[0])<<16)
					| (((int)g_spi.addr[1])<<8)
					| (((int)g_spi.addr[2])<<0);
				LOGF((logfp, "sim:spi addr == %x\n", addr));
				fseek(g_spi.fp, addr, SEEK_SET);
				g_spi.state = sss_read_fetch;
			}
			break;
		case sss_read_fetch:
			result = fgetc(g_spi.fp);
			//LOGF((logfp, "sim:spi read fetch == %x\n", result));
			break;	
	}

	// deliver on stack. This may be a little infinite-recursion-y
	if (g_spi.recursions > 10)
	{
		g_spi.result = result;
	}
	else
	{
		g_spi.spi_ifc->func(g_spi.spi_ifc, result);
	}
	g_spi.recursions -= 1;
}

void sim_spi_poke()
{
	if (!g_spi.initted) { return; }

	//LOGF((logfp, "sim_spi_poke delivering deferred result\n"));
	if (g_spi.result >= 0)
	{
		uint8_t result = g_spi.result;
		g_spi.result = -1;
		g_spi.spi_ifc->func(g_spi.spi_ifc, result);
	}
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

void hal_init_joystick_button()
{
	g_joystick_trigger_state = FALSE;
}

r_bool hal_read_joystick_button()
{
	return g_joystick_trigger_state;
}
