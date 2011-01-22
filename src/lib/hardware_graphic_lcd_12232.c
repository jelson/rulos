#include "hardware_graphic_lcd_12232.h"

// These are in order down the right side (pins 40-21) of a 1284p PDIP.
#define GLCD_RESET	GPIO_A0
#define GLCD_CS1	GPIO_A1
#define GLCD_CS2	GPIO_A2
#define GLCD_RW		GPIO_A3
// NC - a4
#define GLCD_A0		GPIO_A5
#define GLCD_DB0	GPIO_A6
#define GLCD_DB1	GPIO_A7
// nc - aref
// nc - gnd
// nc - avcc
#define GLCD_DB5	GPIO_C7
#define GLCD_DB6	GPIO_C6
#define GLCD_DB7	GPIO_C5
#define GLCD_VBL	GPIO_C4
#define GLCD_DB2	GPIO_C3
#define GLCD_DB3	GPIO_C2
#define GLCD_DB4	GPIO_C1
// nc - c0
// nc - d7

static void delay(uint16_t len)
{
	int i, x;
	for (i=0; i<len; i++)
	{
		x+=1;
	}
}

void glcd_init()
{
	gpio_make_output(GLCD_RESET);
	gpio_make_output(GLCD_CS1);
	gpio_make_output(GLCD_CS2);
	gpio_make_output(GLCD_RW);
	gpio_make_output(GLCD_A0);
	gpio_make_output(GLCD_VBL);
	gpio_set(GLCD_RESET);
	delay(10);
	gpio_clr(GLCD_RESET);
//	gpio_set(GLCD_CS1);
//	gpio_set(GLCD_CS2);
}

#define GLCD_READ	1
#define GLCD_WRITE	0
void glcd_set_bus_dir(r_bool dir)
{
	// TODO life would be faster if hardware had data bus aligned
	// with an 8-bit port!
	if (dir==GLCD_WRITE)
	{
		gpio_make_output(GLCD_DB0);
		gpio_make_output(GLCD_DB1);
		gpio_make_output(GLCD_DB2);
		gpio_make_output(GLCD_DB3);
		gpio_make_output(GLCD_DB4);
		gpio_make_output(GLCD_DB5);
		gpio_make_output(GLCD_DB6);
		gpio_make_output(GLCD_DB7);
	}
	else
	{
		gpio_make_input(GLCD_DB0);
		gpio_make_input(GLCD_DB1);
		gpio_make_input(GLCD_DB2);
		gpio_make_input(GLCD_DB3);
		gpio_make_input(GLCD_DB4);
		gpio_make_input(GLCD_DB5);
		gpio_make_input(GLCD_DB6);
		gpio_make_input(GLCD_DB7);
	}
}

uint8_t glcd_read_data()
{
	uint8_t val = 0
		| (gpio_is_set(GLCD_DB0) ? (1<<0) : 0)
		| (gpio_is_set(GLCD_DB1) ? (1<<1) : 0)
		| (gpio_is_set(GLCD_DB2) ? (1<<2) : 0)
		| (gpio_is_set(GLCD_DB3) ? (1<<3) : 0)
		| (gpio_is_set(GLCD_DB4) ? (1<<4) : 0)
		| (gpio_is_set(GLCD_DB5) ? (1<<5) : 0)
		| (gpio_is_set(GLCD_DB6) ? (1<<6) : 0)
		| (gpio_is_set(GLCD_DB7) ? (1<<7) : 0)
		;
	return val;
}

void glcd_write_data(uint8_t v)
{
	gpio_set_or_clr(GLCD_DB0, (v & (1<<0)));
	gpio_set_or_clr(GLCD_DB1, (v & (1<<1)));
	gpio_set_or_clr(GLCD_DB2, (v & (1<<2)));
	gpio_set_or_clr(GLCD_DB3, (v & (1<<3)));
	gpio_set_or_clr(GLCD_DB4, (v & (1<<4)));
	gpio_set_or_clr(GLCD_DB5, (v & (1<<5)));
	gpio_set_or_clr(GLCD_DB6, (v & (1<<6)));
	gpio_set_or_clr(GLCD_DB7, (v & (1<<7)));
}

void glcd_set_backlight(r_bool vbl)
{
	gpio_make_output(GLCD_VBL);
	gpio_set_or_clr(GLCD_VBL, vbl);
}

uint16_t glcd_read_status(uint8_t bias)
{
	// read
	glcd_set_bus_dir(GLCD_WRITE);
	glcd_write_data(bias);
	gpio_set(GLCD_RW);	// read
	gpio_clr(GLCD_A0);
	glcd_set_bus_dir(GLCD_READ);
	gpio_set(GLCD_CS1);
	delay(10);
	gpio_clr(GLCD_CS1);
	uint8_t v0 = glcd_read_data();
	gpio_set(GLCD_CS2);
	delay(10);
	gpio_clr(GLCD_CS2);
	uint8_t v1 = glcd_read_data();
	return (((uint16_t)v0)<<8) | v1;
}

#define STROBE()	{ \
		gpio_clr(GLCD_CS1); \
		gpio_clr(GLCD_CS2); \
		delay(10); \
		gpio_set(GLCD_CS1); \
		gpio_set(GLCD_CS2); \
		delay(10); \
		}

void syncdebug(uint8_t spaces, char f, uint16_t line);

uint8_t glcd_read_op(r_bool ao)
{
	glcd_set_bus_dir(GLCD_READ);
	gpio_clr(GLCD_CS1);
	gpio_clr(GLCD_CS2);
	gpio_set(GLCD_RW);	// read
	gpio_set_or_clr(GLCD_A0, ao);
	gpio_set(GLCD_CS1);
	uint8_t val = glcd_read_data();
	gpio_clr(GLCD_CS1);
	return val;
}

void glcd_write_op(r_bool ao, uint8_t val)
{
	glcd_set_bus_dir(GLCD_WRITE);
	gpio_clr(GLCD_CS1);
	gpio_clr(GLCD_CS2);
	gpio_clr(GLCD_RW);	// read
	gpio_set_or_clr(GLCD_A0, ao);
	gpio_set(GLCD_CS1);
	glcd_write_data(val);
	gpio_clr(GLCD_CS1);
	glcd_set_bus_dir(GLCD_READ);
}

void glcd_set_display_on(r_bool on)
{
	glcd_write_op(0, 0xae | (on?1:0));
}

void glcd_read_test()
{
	uint8_t page, column, val;
	for (page=0; page<4; page++)
	{
		syncdebug(3, 'p', page);
		glcd_write_op(0, 0xb8 | page);
		glcd_write_op(0, 0x00);

		for (column=0; column<64; column++)
		{
			val = glcd_read_op(1);
			syncdebug(3, 'v', val);
		}
	}
}

void glcd_write_test()
{
	uint8_t page, column, val;
	val = 0;
	for (page=0; page<4; page++)
	{
		syncdebug(3, 'p', page);
		glcd_write_op(0, 0xb8 | page);
		glcd_write_op(0, 0x00);

		for (column=0; column<64; column++)
		{
			val += 1;
			glcd_write_op(1, val);
		}
	}
}
