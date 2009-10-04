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

/* configure a pin as output */
static inline void gpio_make_output(volatile uint8_t *ddr,
									volatile uint8_t *port,
									volatile uint8_t *pin,
									uint8_t bit)
{
	reg_set(ddr, bit);
}

/*
 * configure a pin as input, and enable its internal pullup resistors
 */
static inline void gpio_make_input(volatile uint8_t *ddr,
								   volatile uint8_t *port,
								   volatile uint8_t *pin,
								   uint8_t bit)
{
	reg_clr(ddr, bit);
	reg_set(port, bit);
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
	return ((*pin) & (1 << bit)) == 0;
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
