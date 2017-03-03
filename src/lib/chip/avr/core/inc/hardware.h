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

#pragma once

#include <avr/io.h>
#include <avr/boot.h>
#include <avr/interrupt.h>

#include "hal.h"

#if defined(MCUatmega1284) || defined(MCUatmega1284p)
# define MCU1284_line
#elif defined(MCUatmega88) || defined(MCUatmega88p) || defined(MCUatmega168) || defined(MCUatmega168p) || defined(MCUatmega328) || defined(MCUatmega328p)
# define MCU328_line
#elif defined (MCUatmega8) || defined(MCUatmega16) || defined (MCUatmega32)
# define MCU8_line
#elif defined (MCUattiny24) || defined(MCUattiny44) || defined (MCUattiny84) || defined (MCUattiny24a) || defined(MCUattiny44a) || defined (MCUattiny84a)
# define MCUtiny84_line
#else
# error Unknown processor
#endif

#define GPIO_A0  (&DDRA), (&PORTA), (&PINA), (PORTA0)
#define GPIO_A1  (&DDRA), (&PORTA), (&PINA), (PORTA1)
#define GPIO_A2  (&DDRA), (&PORTA), (&PINA), (PORTA2)
#define GPIO_A3  (&DDRA), (&PORTA), (&PINA), (PORTA3)
#define GPIO_A4  (&DDRA), (&PORTA), (&PINA), (PORTA4)
#define GPIO_A5  (&DDRA), (&PORTA), (&PINA), (PORTA5)
#define GPIO_A6  (&DDRA), (&PORTA), (&PINA), (PORTA6)
#define GPIO_A7  (&DDRA), (&PORTA), (&PINA), (PORTA7)
#define GPIO_B0  (&DDRB), (&PORTB), (&PINB), (PORTB0)
#define GPIO_B1  (&DDRB), (&PORTB), (&PINB), (PORTB1)
#define GPIO_B2  (&DDRB), (&PORTB), (&PINB), (PORTB2)
#define GPIO_B3  (&DDRB), (&PORTB), (&PINB), (PORTB3)
#define GPIO_B4  (&DDRB), (&PORTB), (&PINB), (PORTB4)
#define GPIO_B5  (&DDRB), (&PORTB), (&PINB), (PORTB5)
#define GPIO_B6  (&DDRB), (&PORTB), (&PINB), (PORTB6)
#define GPIO_B7  (&DDRB), (&PORTB), (&PINB), (PORTB7)
#define GPIO_C0  (&DDRC), (&PORTC), (&PINC), (PORTC0)
#define GPIO_C1  (&DDRC), (&PORTC), (&PINC), (PORTC1)
#define GPIO_C2  (&DDRC), (&PORTC), (&PINC), (PORTC2)
#define GPIO_C3  (&DDRC), (&PORTC), (&PINC), (PORTC3)
#define GPIO_C4  (&DDRC), (&PORTC), (&PINC), (PORTC4)
#define GPIO_C5  (&DDRC), (&PORTC), (&PINC), (PORTC5)
#define GPIO_C6  (&DDRC), (&PORTC), (&PINC), (PORTC6)
#define GPIO_C7  (&DDRC), (&PORTC), (&PINC), (PORTC7)
#define GPIO_D0  (&DDRD), (&PORTD), (&PIND), (PORTD0)
#define GPIO_D1  (&DDRD), (&PORTD), (&PIND), (PORTD1)
#define GPIO_D2  (&DDRD), (&PORTD), (&PIND), (PORTD2)
#define GPIO_D3  (&DDRD), (&PORTD), (&PIND), (PORTD3)
#define GPIO_D4  (&DDRD), (&PORTD), (&PIND), (PORTD4)
#define GPIO_D5  (&DDRD), (&PORTD), (&PIND), (PORTD5)
#define GPIO_D6  (&DDRD), (&PORTD), (&PIND), (PORTD6)
#define GPIO_D7  (&DDRD), (&PORTD), (&PIND), (PORTD7)

// For when you simply must store the pin definition dynamically.
typedef struct s_iopindef {
	volatile uint8_t *ddr;
	volatile uint8_t *port;
	volatile uint8_t *pin;
	uint8_t bit;
} IOPinDef;
#define PINDEF(IOPIN)	{IOPIN}
#define PINUSE(IOPIN)	(IOPIN).ddr, (IOPIN).port, (IOPIN).pin, (IOPIN).bit

/*
 * set a bit in a register
 */
static inline void reg_set(volatile uint8_t *reg, uint8_t bit)
{
	*reg |= (1 << bit);
}

/*
 * clear a bit in a register
 */
static inline void reg_clr(volatile uint8_t *reg, uint8_t bit)
{
	*reg &= ~(1 << bit);
}

static inline uint8_t reg_is_clr(volatile uint8_t *reg, uint8_t bit)
{
	return ((*reg) & (1 << bit)) == 0;
}

static inline uint8_t reg_is_set(volatile uint8_t *reg, uint8_t bit)
{
	return !reg_is_clr(reg, bit);
}

/* configure a pin as output */
static inline void gpio_make_output(volatile uint8_t *ddr,
									volatile uint8_t *port,
									volatile uint8_t *pin,
									uint8_t bit)
{
	reg_set(ddr, bit);
}

/*
 * configure a pin as input, without touching the pullup config register
 */
static inline void gpio_make_input(volatile uint8_t *ddr,
								   volatile uint8_t *port,
								   volatile uint8_t *pin,
								   uint8_t bit)
{
	reg_clr(ddr, bit); // set direction as input
}

/*
 * configure a pin as input, and enable its internal pullup resistors
 */
static inline void gpio_make_input_enable_pullup(volatile uint8_t *ddr,
								   volatile uint8_t *port,
								   volatile uint8_t *pin,
								   uint8_t bit)
{
	gpio_make_input(ddr, port, pin, bit);
	reg_set(port, bit); // enable internal pull-up resistors
}

/*
 * configure a pin as input and disable its pullup resistor.
 */
static inline void gpio_make_input_disable_pullup(volatile uint8_t *ddr,
					     volatile uint8_t *port,
					     volatile uint8_t *pin,
					     uint8_t bit)
{
	gpio_make_input(ddr, port, pin, bit);
	reg_clr(port, bit); // disable internal pull-up resistor
}

/*
 * assert an output pin HIGH
 */
static inline void gpio_set(volatile uint8_t *ddr,
			    volatile uint8_t *port,
			    volatile uint8_t *pin,
			    uint8_t bit)
{
	reg_set(port, bit);
}

/*
 * assert an output pin LOW
 */
static inline void gpio_clr(volatile uint8_t *ddr,
							volatile uint8_t *port,
							volatile uint8_t *pin,
							uint8_t bit)
{
	reg_clr(port, bit);
}

/*
 * assert an output either high or low depending on the 'onoff' parameter
 */
static inline void gpio_set_or_clr(volatile uint8_t *ddr,
								   volatile uint8_t *port,
								   volatile uint8_t *pin,
								   uint8_t bit,
								   uint8_t onoff)
{
	if (onoff)
		gpio_set(ddr, port, pin, bit);
	else
		gpio_clr(ddr, port, pin, bit);
}

/*
 * returns true if an input is being asserted LOW, false otherwise
 */
static inline int gpio_is_clr(volatile uint8_t *ddr,
							  volatile uint8_t *port,
							  volatile uint8_t *pin,
							  uint8_t bit)
{
	return reg_is_clr(pin, bit);
}

/*
 * returns true if an input is being asserted HIGH, false otherwise
 */
static inline int gpio_is_set(volatile uint8_t *ddr,
							  volatile uint8_t *port,
							  volatile uint8_t *pin,
							  uint8_t bit)
{
	return !(gpio_is_clr(ddr, port, pin, bit));
}


#define _NOP() do { __asm__ __volatile__ ("nop"); } while (0)

void sensor_interrupt_register_handler(Handler handler);

void hardware_assert(uint16_t line);
void hardware_assign_timer_handler(uint8_t timer_id, Handler handler);
void init_f_cpu(void);
extern uint32_t hardware_f_cpu;
