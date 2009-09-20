/*
 * hardware.c: These functions are only needed for physical display hardware.
 *
 * This file is not compiled by the simulator.
 */
#define V1PCB

#define F_CPU 1000000
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
#if defined(PROTO_BOARD)
	/* put board select on PortB[4..2]	*/
	PORTB = (PORTB & ~(0b111 << 2)) | ((board & 0b111) << 2);

	/* put digit select on PortC[2..0] */
	PORTC = (PORTC & ~(0b111)) | (digit & 0b111);

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
	PORTC = (PORTC & ~(0b111)) | (digit & 0b111);

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



void hw_init()
{
	init_output_pins();
}

/*
 * We run the timer event loop here, instead of directly from interrupt context,
 * because we want the slow processing that comes from timers to run outside of
 * interrupt context -- so that actual high priority interrupts (e.g., ADC reading)
 * can happen in a timely manner.
 */
Handler timer_handler;
volatile uint8_t start_timer_handler = 0;

ISR(TIMER1_COMPA_vect)
{
	start_timer_handler += 1;
}

void hw_run()
{
	program_string(0, "1234");
	while (1)
	{
		if (start_timer_handler > 0)
		{
			start_timer_handler--;
			timer_handler();
		}
	}
}


 /* 
  * 
  * Taken from http://sunge.awardspace.com/binary_clock/binary_clock.html
  
  * Use timer/counter1 to generate clock ticks for events.
  *
  * Using CTC mode (mode 4) with 1/1024 prescaler.  Assuming 1Mhz clock
  * (TODO: adjust this based on actual clock frequency), each tick of the counter
  * is 1ms.
  *
  */
void start_clock_ms(int ms, Handler handler)
{
	timer_handler = handler;
	cli();

	OCR1A = ms;

	/* CTC Mode 4 Prescaler 1024 */
	TCCR1A = 0;
  	TCCR1B = (1<<WGM12)|(1<<CS12)|(1<<CS10); 

 	/* enable output-compare int. */
	TIMSK |= (1<<OCIE1A);

	/* reset counter */
	TCNT1 = 0; 

	/* re-enable interrupts */
	sei();
}
