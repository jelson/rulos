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

#include <stdio.h>

#include "rulos.h"
#include "board_buffer.h"
#include "remote_bbuf.h"

#define BBUF_UNMAPPED_INDEX (0xff)

BoardBuffer *foreground[NUM_PSEUDO_BOARDS];

#if BBDEBUG && SIM
void dump(const char *prefix)
{
	int b;
	LOG("-----\n");
	for (b=0; b<NUM_PSEUDO_BOARDS; b++)
	{
		LOG("%s b%2d: ", prefix, b);
		BoardBuffer *buf = foreground[b];
		while (buf!=NULL)
		{
			LOG("%s ", buf->label);
			buf = buf->next;
		}
		LOG("<end>\n");
	}
}
#endif // BBDEBUG && SIM

RemoteBBufSend *g_remote_bbuf_send;

void board_buffer_module_init()
{
	int i;
	for (i=0; i<NUM_PSEUDO_BOARDS; i++)
	{
		foreground[i] = NULL;
	}
	g_remote_bbuf_send = NULL;
}

void install_remote_bbuf_send(RemoteBBufSend *rbs)
{
	g_remote_bbuf_send = rbs;
}

void board_buffer_init(BoardBuffer *buf DBG_BBUF_LABEL_DECL)
{
	int i;
	for (i=0; i<NUM_DIGITS; i++)
	{
		buf->buffer[i] = 0;
	}
	buf->next = NULL;
	buf->board_index = BBUF_UNMAPPED_INDEX;
	buf->alpha = 0xff;
	assert(sizeof(buf->alpha)*8 >= NUM_DIGITS);
#if BBDEBUG && SIM
	buf->label = label;
#endif
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
	if (board_buffer_is_foreground(buf))
	{
		if (buf->next == NULL)
		{
			// will the last buffer to leave, please turn out the lights?
			program_row(buf->board_index, 0);
			foreground[buf->board_index] = NULL;
		}
		else
		{
			foreground[buf->board_index] = buf->next;
		}
	}
	else
	{
		// TODO if restoring remote display code: this path not implemented.
		assert(buf->board_index<NUM_PSEUDO_BOARDS);
		BoardBuffer *prevbuf = foreground[buf->board_index];
		uint8_t loopcheck=0;
		while (prevbuf->next != buf)
		{
			prevbuf = prevbuf->next;
			assert(prevbuf!=NULL);
			loopcheck += 1;
			assert(loopcheck<100);
		}
		prevbuf->next = buf->next;
	}

	_board_buffer_compute_mask(buf->next, TRUE);	// fyi okay even when buf->next==NULL
	buf->next = NULL;
	buf->board_index = BBUF_UNMAPPED_INDEX;

#if BBDEBUG && SIM
dump("  after pop");
#endif // BBDEBUG && SIM
}

void board_buffer_push(BoardBuffer *buf, int board)
{
	assert(board < NUM_PSEUDO_BOARDS);
#if BBDEBUG && SIM
dump("before push");
#endif // BBDEBUG && SIM
	buf->board_index = board;
	buf->next = foreground[board];
	foreground[board] = buf;
	_board_buffer_compute_mask(buf, FALSE);	// no redraw because...
	board_buffer_draw(buf);	// ...this line will overwrite existing
#if BBDEBUG && SIM
dump(" after push");
#endif // BBDEBUG && SIM
}

void board_buffer_set_alpha(BoardBuffer *buf, uint8_t alpha)
{
	buf->alpha = alpha;

	if (board_buffer_is_stacked(buf))
	{
		_board_buffer_compute_mask(foreground[buf->board_index], TRUE);
	}
}

void board_buffer_draw(BoardBuffer *buf)
{
	uint8_t board_index = buf->board_index;

#if BBDEBUG && SIM
	if (board_index==3)
	{
		LOG("bb_draw(3, buf %016" PRIxPTR " %s, mask %x)\n", (uintptr_t) buf, buf->label, buf->mask);
	}
#endif // BBDEBUG && SIM

	// draw locally, if we can.
	if (0<=board_index && board_index<NUM_BOARDS)
	{
		board_buffer_paint(buf->buffer, board_index, buf->mask);
	}
	else if (g_remote_bbuf_send!=NULL
		&& NUM_BOARDS<=board_index && board_index<NUM_PSEUDO_BOARDS)
	{
		// jonh hard-codes remote send ability, rather than getting all
		// objecty about it, because doing this well with polymorphism
		// really wants a dynamic memory allocator.
		send_remote_bbuf(g_remote_bbuf_send, buf->buffer, board_index-NUM_BOARDS, buf->mask);
	}
}

void board_buffer_paint(SSBitmap *bm, uint8_t board_index, uint8_t mask)
{
	uint8_t idx;
	for (idx=0; idx<NUM_DIGITS; idx++, mask<<=1)
	{
		if (mask & 0x80)
		{
			SSBitmap tmp = bm[idx];
			// 
			program_cell(board_index, idx, tmp);
		}
	}
}

uint8_t board_buffer_is_foreground(BoardBuffer *buf)
{
	return (buf->board_index<NUM_PSEUDO_BOARDS
		&& foreground[buf->board_index] == buf);
}

r_bool board_buffer_is_stacked(BoardBuffer *buf)
{
	return buf->board_index != BBUF_UNMAPPED_INDEX;
}
