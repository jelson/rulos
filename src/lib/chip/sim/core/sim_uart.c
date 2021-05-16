/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
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
#include "core/hal.h"
#include "core/keystrokes.h"
#include "core/rulos.h"
#include "periph/ring_buffer/rocket_ring_buffer.h"
#include "periph/uart/uart.h"

/********************** uart output simulator *********************/

bool sim_uart_keystroke_handler(char k);
static void uart_simulator_input(int c);
static void uart_simulator_stop();
static void draw_uart_input_window();

static WINDOW *uart_input_window = NULL;
char recent_uart_buf[40];

static hal_uart_receive_cb uart_recv_cb = NULL;
static void *uart_user_data = NULL;

void hal_uart_init(uint8_t uart_id, uint32_t baud, bool stop2,
                   void *user_data /* for both rx and tx upcalls */,
                   uint16_t *max_tx_len /* OUT */) {
  *max_tx_len = 3;
  uart_user_data = user_data;
  sim_maybe_init_and_register_keystroke_handler(sim_uart_keystroke_handler);
  memset(recent_uart_buf, 0, sizeof(recent_uart_buf));
}

void hal_uart_start_rx(uint8_t uart_id, hal_uart_receive_cb rx_cb) {
  uart_recv_cb = rx_cb;
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

bool sim_uart_keystroke_handler(char c) {
  switch (c) {
    case 'u':
      uart_simulator_start();
      return true;
    default:
      return false;
  }
}

typedef void (*SimKeystrokeHandler)(char k);

void hal_uart_start_send(uint8_t uart_id, hal_uart_next_sendbuf_cb cb) {
  char buf[4096];
  int i = 0;

  assert(uart_id == 0);
  while (true) {
    uint16_t this_send_len;
    const char *this_buf;
    cb(uart_id, uart_user_data, &this_buf, &this_send_len);
    if (this_send_len > 0) {
      memcpy(&buf[i], this_buf, this_send_len);
      i += this_send_len;
    } else {
      break;
    }
  }

  buf[i + 1] = '\0';

  LOG("Sent to uart %d: %s", uart_id, buf);
}

/********** uart input simulator ***************/

static void draw_uart_input_window() {
  mvwprintw(uart_input_window, 2, 1, "%s", recent_uart_buf);
  wrefresh(uart_input_window);
}

static void uart_simulator_input(int c) {
  LOG("sim inserting to uart: %c", c);

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
  if (uart_recv_cb != NULL) {
    (uart_recv_cb)(0, uart_user_data, c);
  }
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

void hal_uart_init(HalUart* handler, uint32_t baud, bool stop2,
                   uint8_t uart_id)
{
	handler->uart_id = uart_id;
	assert(uart_id<sizeof(sim_uart_fd)/sizeof(sim_uart_fd[0]));
	sim_uart_fd[uart_id] = open("sim_uart", O_RDONLY);
	fprintf(stderr, "open returns %d\n", sim_uart_fd[uart_id]);
	schedule_us(1000, sim_uart_recv, handler);
}

#endif
