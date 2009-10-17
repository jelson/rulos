#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "uart.h"

/************************************************************************************/
/************************************************************************************/


typedef struct {
	ActivationFunc f;
	BoardBuffer bbuf_k;
	BoardBuffer bbuf_u;
} KeyTestActivation_t;


void add_char_to_bbuf(BoardBuffer *bbuf, char c)
{
	memmove(bbuf->buffer, bbuf->buffer+1, NUM_DIGITS-1);
	bbuf->buffer[NUM_DIGITS-1] = ascii_to_bitmap(c);
	board_buffer_draw(bbuf);
}

static void update(KeyTestActivation_t *kta)
{
	schedule_us(50000, (Activation *) kta);

	uint8_t c;

	while ((c = hal_read_keybuf()) != 0) {
		add_char_to_bbuf(&kta->bbuf_k, c);
	}

	while (uart_read(&c)) {
		LOGF((logfp, "got uart char %c\n", c));
		add_char_to_bbuf(&kta->bbuf_u, c);
	}
}


int main()
{
	heap_init();
	util_init();
	hal_init();
	clock_init(10000);
	board_buffer_module_init();
	uart_init(12);

	KeyTestActivation_t kta;
	kta.f = (ActivationFunc) update;
	board_buffer_init(&kta.bbuf_k);
	kta.bbuf_k.upside_down = (1 << 2) | (1 << 4);
	board_buffer_push(&kta.bbuf_k, 0);

	board_buffer_init(&kta.bbuf_u);
	kta.bbuf_u.upside_down = (1 << 2) | (1 << 4);
	board_buffer_push(&kta.bbuf_u, 1);
	schedule_us(1, (Activation *) &kta);

	cpumon_main_loop();
	return 0;
}

