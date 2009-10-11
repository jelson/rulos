#include <stdio.h>

#include "rocket.h"

#if BBDEBUG && SIM
void dump()
{
	int b;
	LOGF((logfp, "-----\n"));
	for (b=0; b<NUM_BOARDS; b++)
	{
		LOGF((logfp, "b%d: ", b));
		BoardBuffer *buf = foreground[b];
		while (buf!=NULL)
		{
			LOGF((logfp, "%s ", buf->label));
			buf = buf->next;
		}
		LOGF((logfp, "<end>\n"));
	}
}
#endif // BBDEBUG && SIM

void board_buffer_module_init()
{
	int i;
	for (i=0; i<NUM_BOARDS; i++)
	{
		foreground[i] = NULL;
	}
}

void board_buffer_init(BoardBuffer *buf)
{
	int i;
	for (i=0; i<NUM_DIGITS; i++)
	{
		buf->buffer[i] = 0;
	}
	buf->next = NULL;
	buf->board_index = 0xff;
	buf->alpha = 0xff;
	assert(sizeof(buf->alpha)*8 >= NUM_DIGITS);
}

void _board_buffer_compute_mask(BoardBuffer *buf, uint8_t redraw)
{
	uint8_t left = 0xff;
	while (buf != NULL)
	{
		buf->mask = left & buf->alpha;
		left = left & (~buf->alpha);
		if (redraw)
		{
			board_buffer_draw(buf);
		}
		buf = buf->next;
	}
}

void board_buffer_pop(BoardBuffer *buf)
{
	if (!board_buffer_is_foreground(buf))
	{
		// we're not actually on top of the display stack.
		// we don't have up links to handle this case.
		assert(0);
	}

	if (buf->next == NULL)
	{
		// will the last buffer to leave, please turn out the lights?
		program_row(buf->board_index, 0);
		foreground[buf->board_index] = NULL;
	}
	else
	{
		_board_buffer_compute_mask(buf->next, TRUE);
		foreground[buf->board_index] = buf->next;
	}
	buf->next = NULL;
	buf->board_index = 0xff;
#if BBDEBUG && SIM
dump();
#endif // BBDEBUG && SIM
}

void board_buffer_push(BoardBuffer *buf, int board)
{
#if BBDEBUG && SIM
LOGF((logfp, "before push\n"));
dump();
#endif // BBDEBUG && SIM
	buf->board_index = board;
	buf->next = foreground[board];
	foreground[board] = buf;
	_board_buffer_compute_mask(buf, FALSE);	// no redraw because...
	board_buffer_draw(buf);	// ...this line will overwrite existing
#if BBDEBUG && SIM
dump();
#endif // BBDEBUG && SIM
}

void board_buffer_set_alpha(BoardBuffer *buf, uint8_t alpha)
{
	buf->alpha = alpha;
	_board_buffer_compute_mask(buf, TRUE);
}

void board_buffer_draw(BoardBuffer *buf)
{
	int board_index = buf->board_index;
	uint8_t mask = buf->mask;
	SSBitmap *bm = buf->buffer;
	uint8_t idx;
	for (idx=0; idx<NUM_DIGITS; idx++, mask<<=1)
	{
		if (mask & 0x80)
		{
			program_cell(board_index, idx, bm[idx]);
		}
	}
}

uint8_t board_buffer_is_foreground(BoardBuffer *buf)
{
	return (buf->board_index<NUM_BOARDS
		&& foreground[buf->board_index] == buf);
}
