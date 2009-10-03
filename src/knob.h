#ifndef _knob_h
#define _knob_h

#include "region.h"
#include "focus.h"

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
