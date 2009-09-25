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
} Knob;

void knob_init(Knob *knob, RowRegion region, char **msgs, uint8_t len, FocusManager *fa);

#endif //_knob_h
