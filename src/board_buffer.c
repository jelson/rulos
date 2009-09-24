#include "board_buffer.h"
#include "util.h"

#include <stdio.h>

SSBitmap _board_buffer_getchar(BoardBuffer *buf, int idx);

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
		buf->buffer[i] = ' ';
	}
	buf->next = NULL;
	buf->board_index = 0xff;
	buf->alpha = 0xff;
	assert(sizeof(buf->alpha)*8 >= NUM_DIGITS);
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
		program_board(buf->board_index, buf->next->buffer);
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
	board_buffer_draw(buf);
#if BBDEBUG && SIM
dump();
#endif // BBDEBUG && SIM
}

void board_buffer_draw(BoardBuffer *buf)
{
	if (board_buffer_is_foreground(buf))
	{
		program_board(buf->board_index, buf->buffer);
		int board_index = buf->board_index;
		int i;
		for (i=0; i<NUM_DIGITS; i++)
		{
			program_cell(board_index, i, _board_buffer_getchar(buf, i));
		}
	}
	// TODO instead of a board-wide if, we should compute the aggregate
	// mask of the overlays above us, and draw through what's left.
	// Without that, updates from below visible through the alpha mask
	// are only shown when the guy on top draws.
}

SSBitmap _board_buffer_getchar(BoardBuffer *buf, int idx)
{
	if ((buf->alpha >> idx) & 1)
	{
		return buf->buffer[idx];
	}
	else if (buf->next != NULL)
	{
		return _board_buffer_getchar(buf->next, idx);
	}
	else
	{
		return 0;
	}
}

uint8_t board_buffer_is_foreground(BoardBuffer *buf)
{
	return (buf->board_index<NUM_BOARDS
		&& foreground[buf->board_index] == buf);
}
