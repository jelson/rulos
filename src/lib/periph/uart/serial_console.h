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

#ifndef _SERIAL_CONSOLE_H
#define _SERIAL_CONSOLE_H

#include "uart.h"

typedef struct {
	UartState_t uart;
	char line[40];
	char *line_ptr;
	ActivationRecord line_act;
} SerialConsole;

void serial_console_init(SerialConsole *sca, ActivationFuncPtr line_func, void *line_data);
void serial_console_sync_send(SerialConsole *act, const char *buf, uint16_t buflen);

#endif //_SERIAL_CONSOLE_H
