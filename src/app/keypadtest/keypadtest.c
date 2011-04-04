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
#include <stdbool.h>

#include "rocket.h"
#include "uart.h"

/************************************************************************************/
/************************************************************************************/


typedef struct {
	BoardBuffer bbuf_k;
	BoardBuffer bbuf_u;
	UartState_t uart;
} KeyTestActivation_t;


void add_char_to_bbuf(BoardBuffer *bbuf, char c)
{
	memmove(bbuf->buffer, bbuf->buffer+1, NUM_DIGITS-1);
	bbuf->buffer[NUM_DIGITS-1] = ascii_to_bitmap(c);
	board_buffer_draw(bbuf);
}

static void update(KeyTestActivation_t *kta)
{
	schedule_us(50000, (ActivationFuncPtr) update, kta);

	uint8_t c;

	while ((c = hal_read_keybuf()) != 0) {
		add_char_to_bbuf(&kta->bbuf_k, c);
	}

	while (uart_read(&kta->uart, (char*) &c)) {
		LOGF((logfp, "got uart char %c\n", c));
		add_char_to_bbuf(&kta->bbuf_u, c);
	}
}


#define TEST_STR "hello world from the uart"

int main()
{
	hal_init();
	init_clock(10000, TIMER1);
	hal_init_keypad();	// requires clock to be initted.
	board_buffer_module_init();


	KeyTestActivation_t kta;
	uart_init(&kta.uart, 38400, true);
	uart_send(&kta.uart, (char*) TEST_STR, strlen((char*) TEST_STR), NULL, NULL);

	board_buffer_init(&kta.bbuf_k);
	board_buffer_push(&kta.bbuf_k, 0);

	board_buffer_init(&kta.bbuf_u);
	board_buffer_push(&kta.bbuf_u, 1);
	schedule_us(1, (ActivationFuncPtr) update, &kta);

	add_char_to_bbuf(&kta.bbuf_k, 'I');
	add_char_to_bbuf(&kta.bbuf_k, 'n');
	add_char_to_bbuf(&kta.bbuf_k, 'i');
	add_char_to_bbuf(&kta.bbuf_k, 't');

	cpumon_main_loop();
	return 0;
}

