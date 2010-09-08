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

#ifndef __board_buffer_h__
#define __board_buffer_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif

#define BBDEBUG 0

typedef struct s_board_buffer {
	uint8_t board_index;
	SSBitmap buffer[NUM_DIGITS];
	uint8_t alpha;
	uint8_t mask;	// visible alpha
	struct s_board_buffer *next;
#if BBDEBUG && SIM
#define DBG_BBUF_LABEL(s)	, s
#define DBG_BBUF_LABEL_DECL	, char *label
	char *label;
#else
#define DBG_BBUF_LABEL(s)	/**/
#define DBG_BBUF_LABEL_DECL	/**/
#endif // BBDEBUG && SIM
} BoardBuffer;

#define NUM_AUX_BOARDS 4
#define NUM_PSEUDO_BOARDS (NUM_BOARDS + NUM_AUX_BOARDS)

BoardBuffer *foreground[NUM_PSEUDO_BOARDS];

void board_buffer_module_init();
struct s_remote_bbuf_send;
void install_remote_bbuf_send(struct s_remote_bbuf_send *rbs);

void board_buffer_init(BoardBuffer *buf DBG_BBUF_LABEL_DECL);
void board_buffer_pop(BoardBuffer *buf);
void board_buffer_push(BoardBuffer *buf, int board);
void board_buffer_set_alpha(BoardBuffer *buf, uint8_t alpha);
void board_buffer_draw(BoardBuffer *buf);
uint8_t board_buffer_is_foreground(BoardBuffer *buf);
r_bool board_buffer_is_stacked(BoardBuffer *buf);

// internal method used by remote_bbuf:
void board_buffer_paint(SSBitmap *bm, uint8_t board_index, uint8_t mask);

#endif // __board_buffer_h__
