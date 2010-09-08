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

#ifndef __knob_h__
#define __knob_h__

typedef struct {
	UIEventHandlerFunc func;
	char **msgs;
	uint8_t len;
	uint8_t selected;
	RowRegion region;
	CursorAct cursor;
	UIEventHandler *notify;
} Knob;

void knob_init(Knob *knob, RowRegion region, char **msgs, uint8_t len, UIEventHandler *notify, FocusManager *fa, char *label);

void knob_set_value(Knob *knob, uint8_t value);

#endif //_knob_h
