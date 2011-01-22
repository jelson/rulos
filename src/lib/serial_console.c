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

#include "serial_console.h"

void serial_console_update(Activation *act)
{
	SerialConsole *sca = (SerialConsole *) act;

	char rcv_chr;
	if (CharQueue_pop(sca->uart.recvQueue.q, &rcv_chr)) {
		*(sca->line_ptr) = rcv_chr;

		sca->line_ptr += 1;
		if (sca->line_ptr == &sca->line[sizeof(sca->line)-1])
		{
			// Buffer full!
			rcv_chr = '\n';
		}
		if (rcv_chr == '\n')
		{
			*(sca->line_ptr) = '\0';
			sca->line_ptr = sca->line;
			(sca->line_act->func)(sca->line_act);
		}
	}

	schedule_us(120, &sca->act);
}


void serial_console_init(SerialConsole *sca, Activation *line_act)
{
	uart_init(&sca->uart, 38400, TRUE);
	sca->act.func = serial_console_update;
	sca->line_ptr = sca->line;
	sca->line_act = line_act;

	schedule_us(1000, &sca->act);
}

void serial_console_sync_send(SerialConsole *act, char *buf, uint16_t buflen)
{
	while (uart_busy(&act->uart)) { }
	uart_send(&act->uart, buf, buflen, NULL, NULL);
	while (uart_busy(&act->uart)) { }
}

