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
#include "core/rulos.h"
#include "periph/ring_buffer/rocket_ring_buffer.h"
#include "periph/uart/uart.h"

UartHandler *g_sim_uart_handler = NULL;


/********************** uart output simulator *********************/


void hal_uart_start_send(UartHandler *handler)
{
	char buf[256];
	int i = 0;

	while ((g_sim_uart_handler->send)(g_sim_uart_handler, &buf[i]))
		i++;

	buf[i+1] = '\0';

	LOG("Sent to uart: '%s'\n", buf);
}

void hal_uart_init(UartHandler *s, uint32_t baud, r_bool stop2, uint8_t uart_id)
{
	g_sim_uart_handler = s;
}
