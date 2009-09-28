/*
 * hardware.c: These functions are only needed for physical display hardware.
 *
 * This file is not compiled by the simulator.
 */
#define V1PCB

/* clock calibration by jelson: took 209 seconds to lose one second */
#define F_CPU (4000000 * 209 / 210)
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "rocket.h"
#include "display_controller.h"

#define SET(port, pin) do { port |= (unsigned char) (1 << pin); } while (0)
#define CLR(port, pin) do { port &= (unsigned char) ~(1 << pin); } while (0)
#define IS_SET(port, pin) ((port & (1 << pin)) == (1 << pin))
#define IS_CLR(port, pin) ((port & (1 << pin)) == 0)

#define _NOP() do { __asm__ __volatile__ ("nop"); } while (0)


/*
 * Set output directions on pins that are outputs 
 */
void init_output_pins()
{
#if defined(PROTO_BOARD)
	/* board select outputs */
	SET(DDRB, PORTB2);
	SET(DDRB, PORTB3);
	SET(DDRB, PORTB4);

	/* digit select */
	SET(DDRC, PORTC0);
	SET(DDRC, PORTC1);
	SET(DDRC, PORTC2);

	/* segment select */
	SET(DDRD, PORTD5);
	SET(DDRD, PORTD6);
	SET(DDRD, PORTD7);

	/* data */
	SET(DDRB, PORTB0);

	/* strobe */
	SET(DDRB, PORTB1);
#elif defined(V1PCB)
	/* board select outputs */
	SET(DDRB, PORTB2);
	SET(DDRB, PORTB3);
	SET(DDRB, PORTB4);

	/* digit select */
	SET(DDRC, PORTC0);
	SET(DDRC, PORTC1);
	SET(DDRC, PORTC2);

	/* segment select */
	SET(DDRD, PORTD5);
	SET(DDRD, PORTD6);
	SET(DDRD, PORTD7);

	/* data */
	SET(DDRD, PORTD4);

	/* strobe */
	SET(DDRB, PORTB1);
#else
# error Board type not defined!
#endif
}


void program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff)
{
	uint8_t rdigit = NUM_DIGITS - 1 - digit;
#if defined(PROTO_BOARD)
	/* put board select on PortB[4..2]	*/
	PORTB = (PORTB & ~(0b111 << 2)) | ((board & 0b111) << 2);

	/* put digit select on PortC[2..0] */
	PORTC = (PORTC & ~(0b111)) | (rdigit & 0b111);

	/* put segment select on PortD[7..5] */
	PORTD = (PORTD & ~(0b111 << 5)) | ((segment & 0b111) << 5);

	/* put data on PortB[0].  Sense reversed because cathode is common. */
	PORTB = (PORTB & (~0b1)) | !onoff;

	/* strobe segment program enable on PortB[1] */
	PORTB &= ~(0b10);
	_NOP();
	PORTB |= 0b10;
#elif defined(V1PCB)
	/* put board select on PortB[4..2]	*/
	PORTB = (PORTB & ~(0b111 << 2)) | ((board & 0b111) << 2);

	/* put digit select on PortC[2..0] */
	PORTC = (PORTC & ~(0b111)) | (rdigit & 0b111);

	/*
	 * put segment select on PortD[7..5], and Data on Portd[4].  Sense of onoff reversed
	 * because cathode is common.
	 */
	PORTD = (PORTD & ~(0b1111 << 4)) | ((segment & 0b111) << 5) | ((!onoff) << 4);

	/* strobe segment program enable on PortB[1] */
	PORTB &= ~(0b10);
	_NOP();
	PORTB |= 0b10;
#else
# error  Board type not set!
#endif /* board type */
}


void delay_ms(int ms)
{
	//_delay_ms(ms);
}

void hal_init()
{
	init_output_pins();
}

Handler timer_handler;

ISR(TIMER1_COMPA_vect)
{
	timer_handler();
}



 /* 
  * 
  * Based on http://sunge.awardspace.com/binary_clock/binary_clock.html
  *   
  * Use timer/counter1 to generate clock ticks for events.
  *
  * Using CTC mode (mode 4) with 1/64 prescaler.
  *
  * 64 counter ticks takes 64/F_CPU seconds.  Therefore, to get an s-second
  * interrupt, we need to take one every s / (64/F_CPU) ticks.
  *
  * To avoid floating point, do computation in msec rather than seconds:
  * take an interrupt every ms / (64 / F_CPU / 1000) counter ticks, 
  * or, (ms * F_CPU) / (64 * 1000).
  */
void start_clock_ms(int ms, Handler handler)
{
	long ticks_tmp = ms * F_CPU;
	ticks_tmp /= 64000;

	timer_handler = handler;
	cli();

	OCR1A = (unsigned int) ticks_tmp;

	/* CTC Mode 4 Prescaler 64 */
	TCCR1A = 0;
  	TCCR1B = (1<<WGM12)|(1<<CS11)|(1<<CS10); 

 	/* enable output-compare int. */
	TIMSK |= (1<<OCIE1A);

	/* reset counter */
	TCNT1 = 0; 

	/* re-enable interrupts */
	sei();
}

#if defined(V1PCB)
uint8_t scan_row()
{
        /*
         * Scan PD0..3.  Return 1..4 if any of them are low.
         * Return 0 if they're all high.
         */
        _NOP();
        if (IS_CLR(PIND, PORTD0))               return 1;
        if (IS_CLR(PIND, PORTD1))               return 2;
        if (IS_CLR(PIND, PORTD2))               return 3;
        if (IS_CLR(PIND, PORTD3))               return 4;
        return 0;
}


char scan_keyboard()
{
        /*
         * (0 asserted on these)
         * Row 1: PC3
         * Row 2: PB0
         * Row 3: PB2
         * Row 4: PB3
         *
         * (inputs with pull-ups are checked)
         * Col 1: PB4
         * Col 2: PC0
         * Col 3: PC1
         * Col 4: PC2
         */

        uint8_t row;

        /* Scan first row */
        CLR(PORTD, PORTD4);
        SET(PORTB, PORTB2);
        SET(PORTB, PORTB3);
        SET(PORTB, PORTB4);
        row = scan_row();
        if (row == 1)           return '1';
        if (row == 2)           return '2';
        if (row == 3)           return '3';
        if (row == 4)           return 'a';

        /* Scan second row */
        SET(PORTD, PORTD4);
        CLR(PORTB, PORTB2);
        SET(PORTB, PORTB3);
        SET(PORTB, PORTB4);
        row = scan_row();
        if (row == 1)           return '4';
        if (row == 2)           return '5';
        if (row == 3)           return '6';
        if (row == 4)           return 'b';

        /* Scan third row */
        SET(PORTD, PORTD4);
        SET(PORTB, PORTB2);
        CLR(PORTB, PORTB3);
        SET(PORTB, PORTB4);
        row = scan_row();
        if (row == 1)           return '7';
        if (row == 2)           return '8';
        if (row == 3)           return '9';
        if (row == 4)           return 'c';

        /* Scan fourth row */
        SET(PORTD, PORTD4);
        SET(PORTB, PORTB2);
        SET(PORTB, PORTB3);
        CLR(PORTB, PORTB4);
        row = scan_row();
        if (row == 1)           return 's';
        if (row == 2)           return '0';
        if (row == 3)           return 'p';
        if (row == 4)           return 'd';

        return 0;
}
#endif //V1PCB


void hal_start_atomic()
{
	cli();
}

void hal_end_atomic()
{
	sei();
}

void hal_idle()
{
	// just busy-wait on microcontroller.
}

