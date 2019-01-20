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
#include "core/keystrokes.h"
#include "core/rulos.h"
#include "periph/ring_buffer/rocket_ring_buffer.h"
#include "periph/uart/uart.h"

/********************** uart output simulator *********************/

r_bool sim_uart_keystroke_handler(char k);
static void uart_simulator_input(int c);
static void uart_simulator_stop();
static void draw_uart_input_window();

HalUart *g_sim_uart_handler = NULL;
extern HalUart *g_sim_uart_handler;
static WINDOW *uart_input_window = NULL;
char recent_uart_buf[20];

void hal_uart_init(HalUart *uart, uint32_t baud, r_bool stop2,
                   uint8_t uart_id) {
  g_sim_uart_handler = uart;
  uart->uart_id = uart_id;
  sim_maybe_init_and_register_keystroke_handler(sim_uart_keystroke_handler);
  memset(recent_uart_buf, 0, sizeof(recent_uart_buf));
}

static void uart_simulator_start() {
  // set up handlers
  sim_install_modal_handler(uart_simulator_input, uart_simulator_stop);

  // create input window
  uart_input_window = newwin(4, sizeof(recent_uart_buf) + 1, 2, 27);
  mvwprintw(uart_input_window, 1, 1, "UART Input Mode:");
  box(uart_input_window, ACS_VLINE, ACS_HLINE);
  draw_uart_input_window();
}

r_bool sim_uart_keystroke_handler(char c) {
  switch (c) {
    case 'u':
      uart_simulator_start();
      return true;
    default:
      return false;
  }
}

typedef void (*SimKeystrokeHandler)(char k);

void hal_uart_start_send(HalUart *uart) {
  char buf[256];
  int i = 0;

  while ((uart->send)(uart, &buf[i])) {
    i++;
  }

  buf[i + 1] = '\0';

  LOG("Sent to uart %d: %s", uart->uart_id, buf);
}

void hal_uart_sync_send_bytes(HalUart *uart, const char *s, uint8_t len) {
  char buf[len + 1];
  memcpy(buf, s, len);
  buf[len] = '\0';
  LOG("Sent to uart %d: %s", uart->uart_id, buf);
}

void hal_uart_sync_send(HalUart *uart, const char *s) {
  LOG("Sent to uart %d: %s", uart->uart_id, s);
}

/********** uart input simulator ***************/

static void draw_uart_input_window() {
  mvwprintw(uart_input_window, 2, 1, "%s", recent_uart_buf);
  wrefresh(uart_input_window);
}

static void uart_simulator_input(int c) {
  LOG("sim inserting to uart: %c\n", c);

  // display on screen
  if (strlen(recent_uart_buf) == sizeof(recent_uart_buf) - 1) {
    memmove(&recent_uart_buf[0], &recent_uart_buf[1],
            sizeof(recent_uart_buf) - 1);
  }
  if (isprint(c))
    sprintf(recent_uart_buf + strlen(recent_uart_buf), "%c", c);
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

static void uart_simulator_stop() { delwin(uart_input_window); }

#if 0
Jon: I disabled this because it conflicted with the 
interactive uart simulator I wrote, in sim_rocketpanel.c.


/* recv-only uart simulator */

int sim_uart_fd[2];

void sim_uart_recv(void *data)
{
	HalUart* uart_handler = (HalUart*) data;
	char c;
	int rc = read(sim_uart_fd[uart_handler->uart_id], &c, 1);
	if (rc==1)
	{
		//fprintf(stderr, "sim_uart_recv(%c)\n", c);
		(uart_handler->recv)(uart_handler, c);
		schedule_us(1000, sim_uart_recv, uart_handler);
	}
	else
	{
		fprintf(stderr, "EOF on sim_uart_fd\n");
	}
}

void hal_uart_init(HalUart* handler, uint32_t baud, r_bool stop2, uint8_t uart_id)
{
	handler->uart_id = uart_id;
	assert(uart_id<sizeof(sim_uart_fd)/sizeof(sim_uart_fd[0]));
	sim_uart_fd[uart_id] = open("sim_uart", O_RDONLY);
	fprintf(stderr, "open returns %d\n", sim_uart_fd[uart_id]);
	schedule_us(1000, sim_uart_recv, handler);
}

#endif
