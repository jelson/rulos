#ifndef _display_blinker_h
#define _display_blinker_h

#include "board_buffer.h"
#include "clock.h"

typedef struct {
	ActivationFunc func;
	uint16_t periodBit;
	const char **msg;
	uint8_t cur_line;
	BoardBuffer bbuf;
} DBlinker;

void blinker_init(DBlinker *blinker, uint8_t period2);
void blinker_set_msg(DBlinker *blinker, const char **msg);

#endif // _display_blinker_h
