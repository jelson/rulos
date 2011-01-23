#include "hardware_graphic_lcd_12232.h"

#include <avr/pgmspace.h>
#include "lcdrasters_auto.ch"

//////////////////////////////////////////////////////////////////////////////
// Board I/O mapping defs
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
//////////////////////////////////////////////////////////////////////////////

void glcd_complete_init(Activation *act);

//////////////////////////////////////////////////////////////////////////////
// kudos for reference code from:
// http://en.radzio.dxp.pl/sed1520/sed1520.zip

#define DISPLAY_WIDTH				122

#define GLCD_EVENT_BUSY				0x80
#define GLCD_EVENT_RESET			0x10

#define GLCD_CMD_DISPLAY_ON			0xAF
#define GLCD_CMD_DISPLAY_OFF		0xAE
#define GLCD_CMD_DISPLAY_START_LINE	0xC0
#define GLCD_CMD_RESET				0xE2
#define GLCD_CMD_SET_PAGE			0xb8
#define GLCD_CMD_SET_COLUMN			0x00

#define GLCD_READ       1
#define GLCD_WRITE      0

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

uint8_t glcd_data_in()
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

void glcd_data_out(uint8_t v)
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

void glcd_wait_status_event(uint8_t event, uint8_t unit)
{
	gpio_set(GLCD_RW);	// read
	gpio_clr(GLCD_A0);	// cmd
	glcd_set_bus_dir(GLCD_READ);
	while (1)
	{
		uint8_t status;
		if (unit == 0) 
		{
			gpio_set(GLCD_CS1);
			asm("nop");asm("nop"); 
			status = glcd_data_in();
			gpio_clr(GLCD_CS1);
		} 
		else 
		{
			gpio_set(GLCD_CS2);
			asm("nop");asm("nop"); 
			status = glcd_data_in();
			gpio_clr(GLCD_CS2);
		}
		if ((status & event)==0)
		{
			break;
		}
	}
	glcd_set_bus_dir(GLCD_WRITE);
}

static inline void glcd_strobe(uint8_t unit)
{
	if (unit)
	{
		gpio_set(GLCD_CS2);
		asm("nop");asm("nop");
		gpio_clr(GLCD_CS2);
	}
	else
	{
		gpio_set(GLCD_CS1);
		asm("nop");asm("nop");
		gpio_clr(GLCD_CS1);
	}
}

void glcd_write(uint8_t val, uint8_t unit, uint8_t a0)
{
	glcd_wait_status_event(GLCD_EVENT_BUSY, unit);

	gpio_clr(GLCD_RW);	// write
	gpio_set_or_clr(GLCD_A0, a0);
	glcd_data_out(val);
	glcd_strobe(unit);
}

static void glcd_write_cmd(uint8_t cmd, uint8_t unit)
{
	glcd_write(cmd, unit, 0);
}

static void glcd_write_data(uint8_t data, uint8_t unit)
{
	glcd_write(data, unit, 1);
}

void glcd_init(GLCD *glcd, Activation *done_act)
{
	glcd->done_act = done_act;

	// set up fixed I/O pins
	glcd_set_bus_dir(GLCD_WRITE);
	gpio_make_output(GLCD_CS1);
	gpio_make_output(GLCD_CS2);
	gpio_make_output(GLCD_RW);
	gpio_make_output(GLCD_A0);
	gpio_make_output(GLCD_RESET);

	// start hardware reset
	gpio_clr(GLCD_RESET);

	// let rest of system run during 10ms delay
	glcd->act.func = glcd_complete_init;
	schedule_us(10000, &glcd->act);
}

void glcd_complete_init(Activation *act)
{
	// complete hardware reset
	GLCD *glcd = (GLCD*) act;
	gpio_set(GLCD_RESET);

	// request software reset
	glcd_write_cmd(GLCD_CMD_RESET, 0);
	glcd_write_cmd(GLCD_CMD_RESET, 1);
	glcd_wait_status_event(GLCD_EVENT_RESET, 0);
	glcd_wait_status_event(GLCD_EVENT_RESET, 1);

	glcd_write_cmd(GLCD_CMD_DISPLAY_ON, 0);
	glcd_write_cmd(GLCD_CMD_DISPLAY_ON, 1);
	/*
	glcd_write_cmd(GLCD_CMD_DISPLAY_START_LINE | 0, 0);
	glcd_write_cmd(GLCD_CMD_DISPLAY_START_LINE | 0, 1);
	*/

	// turn on the backlight
	gpio_set(GLCD_VBL);

	{
		glcd_clear_framebuffer(glcd);
		glcd->framebuffer[0][3+0] = 0x80;
		glcd->framebuffer[1][3+0] = 0xc0;
		glcd->framebuffer[2][3+0] = 0xe0;
		glcd->framebuffer[3][3+0] = 0xf0;
		glcd->framebuffer[4][3+0] = 0xf8;
		glcd->framebuffer[5][3+0] = 0xfc;
		glcd->framebuffer[6][3+0] = 0xfe;
		glcd->framebuffer[7][3+0] = 0xff;
		glcd->framebuffer[8+0][3+1] = 0x80;
		glcd->framebuffer[8+1][3+1] = 0xc0;
		glcd->framebuffer[8+2][3+1] = 0xe0;
		glcd->framebuffer[8+3][3+1] = 0xf0;
		glcd->framebuffer[8+4][3+1] = 0xf8;
		glcd->framebuffer[8+5][3+1] = 0xfc;
		glcd->framebuffer[8+6][3+1] = 0xfe;
		glcd->framebuffer[8+7][3+1] = 0xff;
		glcd->framebuffer[16+0][3+2] = 0x80;
		glcd->framebuffer[16+1][3+2] = 0xc0;
		glcd->framebuffer[16+2][3+2] = 0xe0;
		glcd->framebuffer[16+3][3+2] = 0xf0;
		glcd->framebuffer[16+4][3+2] = 0xf8;
		glcd->framebuffer[16+5][3+2] = 0xfc;
		glcd->framebuffer[16+6][3+2] = 0xfe;
		glcd->framebuffer[16+7][3+2] = 0xff;
	}
	glcd_draw_framebuffer(glcd);

#if 0
	// test pattern
	int i;
	for (i=0; i<32; i++)
	{
		GLCD_SetPixel(i, i, i&1);
	}
#endif

	if (glcd->done_act!=NULL)
	{
		schedule_now(glcd->done_act);
	}
}

#if 0
void glcd_clear_screen(GLCD *glcd)
{
	uint8_t unit, page, column;
	for (unit=0; unit<2; unit++)
	{
		for (page=0; page<4; page++)
		{
			glcd_write_cmd(GLCD_CMD_SET_PAGE | page, unit);
			glcd_write_cmd(GLCD_CMD_SET_COLUMN | 0, unit);
			for (column=0; column<122/2; column++)
			{
				glcd_write_data(0, unit);
			}
		}
	}
}
#endif

void syncdebug(uint8_t spaces, char f, uint16_t line);

static uint8_t _glcd_fetch_column(GLCD *glcd, uint8_t page, uint8_t col)
{
	uint8_t y_base = page << 3;
	uint8_t x_byte = col>>3;
	uint8_t x_bit_shift = 7-(col&0x7);
	return 0
		| (((glcd->framebuffer[y_base+0][x_byte] >> x_bit_shift)&1)<< 0)
		| (((glcd->framebuffer[y_base+1][x_byte] >> x_bit_shift)&1)<< 1)
		| (((glcd->framebuffer[y_base+2][x_byte] >> x_bit_shift)&1)<< 2)
		| (((glcd->framebuffer[y_base+3][x_byte] >> x_bit_shift)&1)<< 3)
		| (((glcd->framebuffer[y_base+4][x_byte] >> x_bit_shift)&1)<< 4)
		| (((glcd->framebuffer[y_base+5][x_byte] >> x_bit_shift)&1)<< 5)
		| (((glcd->framebuffer[y_base+6][x_byte] >> x_bit_shift)&1)<< 6)
		| (((glcd->framebuffer[y_base+7][x_byte] >> x_bit_shift)&1)<< 7)
		;
}

void glcd_clear_framebuffer(GLCD *glcd)
{
	int r, b;
	for (r=0; r<BITMAP_NUM_ROWS; r++)
	{
		for (b=0; b<BITMAP_ROW_LEN; b++)
		{
			glcd->framebuffer[r][b] = 0;
		}
	}
}

void glcd_draw_framebuffer(GLCD *glcd)
{
	uint8_t page, column;
	for (page=0; page<4; page++)
	{
		syncdebug(2, 'p', page);
		glcd_write_cmd(GLCD_CMD_SET_PAGE | page, 0);
		glcd_write_cmd(GLCD_CMD_SET_COLUMN | 0, 0);
		for (column=0; column<(DISPLAY_WIDTH/2); column++)
		{
			uint8_t colval = _glcd_fetch_column(glcd, page, column);
			glcd_write_data(colval, 0);
		}
		glcd_write_cmd(GLCD_CMD_SET_PAGE | page, 1);
		glcd_write_cmd(GLCD_CMD_SET_COLUMN | 0, 1);
		for (column=(DISPLAY_WIDTH/2); column<DISPLAY_WIDTH; column++)
		{
			uint8_t colval = _glcd_fetch_column(glcd, page, column);
			glcd_write_data(colval, 1);
		}
	}
}

uint8_t _glcd_find_glyph_index(char glyph)
{
	uint8_t index;
	for (index = 0; index<lcd_num_syms; index++)
	{
		uint8_t table_glyph = pgm_read_byte(lcd_index_to_sym+index);
		if (table_glyph==glyph)
		{
			return index;
		}
	}
	return 0xff;
}

#define SYNCDEBUG()	syncdebug(0, 'G', __LINE__)

uint8_t glcd_paint_char(GLCD *glcd, char glyph, uint8_t dx0)
{
	uint8_t index = _glcd_find_glyph_index(glyph);
	syncdebug(0, 'i', index);
	if (index==0xff)
	{
		syncdebug(16, 'f', 0xfefe);
		return 0;
	}

	// glyph addresses
	uint16_t gx0 = pgm_read_word(((uint16_t*)lcd_index_to_x)+index);
	uint16_t gx1 = pgm_read_word(((uint16_t*)lcd_index_to_x)+(index+1));

	uint16_t y, gx, dx;
	uint8_t *dptr;
	for (y=0; y<32; y++)
	{
		dptr = glcd->framebuffer[y];
		uint16_t g_row_base = lcd_row_width_bytes * y;
		for (gx=gx0, dx=dx0; gx<gx1 && dx<DISPLAY_WIDTH; gx++, dx++)
		{
			uint16_t g_byte = g_row_base + (gx>>3);
			uint8_t g_data = ~pgm_read_byte(lcd_data+g_byte);
			uint8_t bit = (g_data >> (7-(gx&7))) & 1;
			dptr[dx>>3] |= (bit<<(7-(dx&7)));
		}
	}
	return dx-dx0;
}
