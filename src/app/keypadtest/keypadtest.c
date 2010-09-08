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

	while (uart_read(RULOS_UART0, &c)) {
		LOGF((logfp, "got uart char %c\n", c));
		add_char_to_bbuf(&kta->bbuf_u, c);
	}
}


#define TEST_STR "hello world from the uart"

int main()
{
	heap_init();
	util_init();
	hal_init(bc_rocket0);
	hal_init_keypad();
	init_clock(10000, TIMER1);
	board_buffer_module_init();
	uart_init(RULOS_UART0, 12);


	uart_send(RULOS_UART0, TEST_STR, strlen(TEST_STR), NULL, NULL);

	KeyTestActivation_t kta;
	kta.f = (ActivationFunc) update;

	board_buffer_init(&kta.bbuf_k);
	board_buffer_push(&kta.bbuf_k, 0);

	board_buffer_init(&kta.bbuf_u);
	board_buffer_push(&kta.bbuf_u, 1);
	schedule_us(1, (Activation *) &kta);

	add_char_to_bbuf(&kta.bbuf_k, 'I');
	add_char_to_bbuf(&kta.bbuf_k, 'n');
	add_char_to_bbuf(&kta.bbuf_k, 'i');
	add_char_to_bbuf(&kta.bbuf_k, 't');

	cpumon_main_loop();
	return 0;
}

