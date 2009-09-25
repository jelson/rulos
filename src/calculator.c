#include "calculator.h"

void calculator_init(Calculator *calc, int board0, FocusAct *fa)
{
	calc->func = 
	focus_init(&calc->focus);
	int i;
	for (i=0; i<2; i++)
	{
		board_buffer_init(&calc->bbuf[i]);
		board_buffer_push(&calc->bbuf[i], board0+i);
	}
	RowRegion region = { calc->bbuf[0], 0, 4 };
	numeric_input_init(&calc->operands[0], region, &calc->bbuf[0], &calc->focus);
	RowRegion region = { calc->bbuf[0], 4, 4 };
	numeric_input_init(&calc->operands[1], region, &calc->bbuf[0], &calc->focus);
	RowRegion region = { calc->bbuf[1], 0, 4 };
	operator_init(&calc->operator, region, &calc->focus);

	NumericInputAct result;
}
