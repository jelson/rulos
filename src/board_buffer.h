#ifndef __board_buffer_h__
#define __board_buffer_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif

typedef struct s_board_buffer {
	uint8_t board_index;
	SSBitmap buffer[NUM_DIGITS];
	uint8_t alpha;
	uint8_t mask;	// visible alpha
	struct s_board_buffer *next;
#if BBDEBUG && SIM
	char *label;
#endif // BBDEBUG && SIM
} BoardBuffer;

BoardBuffer *foreground[NUM_BOARDS];

void board_buffer_module_init();

void board_buffer_init(BoardBuffer *buf);
void board_buffer_pop(BoardBuffer *buf);
void board_buffer_push(BoardBuffer *buf, int board);
void board_buffer_set_alpha(BoardBuffer *buf, uint8_t alpha);
void board_buffer_draw(BoardBuffer *buf);
uint8_t board_buffer_is_foreground(BoardBuffer *buf);

#endif // __board_buffer_h__
