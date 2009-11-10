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
	uint8_t upside_down; // LEDs mounted upside-down
	struct s_board_buffer *next;
#if BBDEBUG && SIM
	char *label;
#endif // BBDEBUG && SIM
} BoardBuffer;

#define NUM_AUX_BOARDS 4
#define NUM_PSEUDO_BOARDS (NUM_BOARDS + NUM_AUX_BOARDS)

BoardBuffer *foreground[NUM_PSEUDO_BOARDS];

void board_buffer_module_init();
struct s_remote_bbuf_send;
void install_remote_bbuf_send(struct s_remote_bbuf_send *rbs);

void board_buffer_init(BoardBuffer *buf);
void board_buffer_pop(BoardBuffer *buf);
void board_buffer_push(BoardBuffer *buf, int board);
void board_buffer_set_alpha(BoardBuffer *buf, uint8_t alpha);
void board_buffer_draw(BoardBuffer *buf);
uint8_t board_buffer_is_foreground(BoardBuffer *buf);
r_bool board_buffer_is_stacked(BoardBuffer *buf);

// internal method used by remote_bbuf:
void board_buffer_paint(SSBitmap *bm, uint8_t board_index, uint8_t mask);

#endif // __board_buffer_h__
