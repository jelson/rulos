#include "util.h"
#include "calculator.h"

char *operator_strs[] = {
	"add", "sub", "mul", "div" };

void calculator_init(Calculator *calc, int board0, FocusManager *fa)
{
	focus_init(&calc->focus);

	int i;
	for (i=0; i<2; i++)
	{
		board_buffer_init(&calc->bbuf[i]);
		board_buffer_push(&calc->bbuf[i], board0+i);
		calc->btable[i] = &calc->bbuf[i];
	}

	RectRegion calc_region = { calc->btable, 2, 1, 6 };
	focus_register(fa, (UIEventHandler*) &calc->focus, calc_region);

	RowRegion region0 = { &calc->bbuf[0], 0, 4 };
	numeric_input_init(&calc->operands[0], region0, &calc->focus);
	RowRegion region1 = { &calc->bbuf[0], 4, 4 };
	numeric_input_init(&calc->operands[1], region1, &calc->focus);
	RowRegion region2 = { &calc->bbuf[1], 0, 4 };
	knob_init(&calc->operator, region2, operator_strs, 4, &calc->focus);
	RowRegion region3 = { &calc->bbuf[1], 4, 4 };
	numeric_input_init(&calc->result, region3, NULL /*unfocusable*/);
}
