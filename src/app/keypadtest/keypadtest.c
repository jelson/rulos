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


void add_char_to_bbuf(BoardBuffer *bbuf, char c, UartState_t *uart)
{
	memmove(bbuf->buffer, bbuf->buffer+1, NUM_DIGITS-1);
	bbuf->buffer[NUM_DIGITS-1] = ascii_to_bitmap(c);
	board_buffer_draw(bbuf);

	char out[100];
	memset(out, 'x', sizeof(out));
	strcpy(out, "boing! ");
	out[7] = c;
	out[8] = '\n';
	out[9] = '\0';
	while (uart_busy(uart));
	uart_send(uart, out, strlen(out), NULL, NULL);
	while (uart_busy(uart));
}

static void update(KeyTestActivation_t *kta)
{
	schedule_us(50000, (ActivationFuncPtr) update, kta);

	uint8_t c;

	while ((c = hal_read_keybuf()) != 0) {
		add_char_to_bbuf(&kta->bbuf_k, c, &kta->uart);
	}

	while (uart_read(&kta->uart, (char*) &c)) {
		LOGF((logfp, "got uart char %c\n", c));
		add_char_to_bbuf(&kta->bbuf_u, c, &kta->uart);
	}
}


#define TEST_STR "hello world from the uart\n"

typedef struct {
	BoardBuffer *bbuf;
	UartState_t *uart;
	int count;
} Metronome;

void tock(Metronome *m)
{
	m->count = (m->count + 1) & 7;
	m->bbuf->buffer[0] = ascii_to_bitmap('0'+m->count);
	board_buffer_draw(m->bbuf);
}

void tick(Metronome *m)
{
	char *out = (char*) "tick\n";
	while (uart_busy(m->uart));
	uart_send(m->uart, out, strlen(out), NULL, NULL);
	while (uart_busy(m->uart));
	schedule_us( 500000, (ActivationFuncPtr) tock, m);
	schedule_us(1000000, (ActivationFuncPtr) tick, m);
}

int main()
{
	hal_init();
	init_clock(10000, TIMER1);
	hal_init_keypad();	// requires clock to be initted.
	board_buffer_module_init();
	hal_init_rocketpanel(bc_chaseclock);


	KeyTestActivation_t kta;
	uart_init(&kta.uart, 38400, true, 0);
	uart_send(&kta.uart, (char*) TEST_STR, strlen((char*) TEST_STR), NULL, NULL);

	board_buffer_init(&kta.bbuf_k);
	board_buffer_push(&kta.bbuf_k, 0);

	board_buffer_init(&kta.bbuf_u);
	board_buffer_push(&kta.bbuf_u, 1);
	schedule_us(1, (ActivationFuncPtr) update, &kta);

	add_char_to_bbuf(&kta.bbuf_k, 'I', &kta.uart);
	add_char_to_bbuf(&kta.bbuf_k, 'n', &kta.uart);
	add_char_to_bbuf(&kta.bbuf_k, 'i', &kta.uart);
	add_char_to_bbuf(&kta.bbuf_k, 't', &kta.uart);

	Metronome m;
	m.uart = &kta.uart;
	m.bbuf = &kta.bbuf_k;
	schedule_us(1000000, (ActivationFuncPtr) tick, &m);

	cpumon_main_loop();
	return 0;
}

