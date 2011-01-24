#include "led_matrix_single.h"

#define LMS_OUTPUT_ENABLE_INV	GPIO_D6
#define LMS_DATA				GPIO_D5
#define LMS_SHIFT_CLOCK			GPIO_D4
#define LMS_LATCH_ENABLE_COLS	GPIO_D3
#define LMS_LATCH_ENABLE_ROWS	GPIO_D2

void _lms_shift_byte(uint8_t data);
void _lms_configure(uint8_t rowdata, uint16_t coldata);

#if !SIM
#include "hardware.h"

void led_matrix_single_init(LEDMatrixSingle *lms)
{
	gpio_set(LMS_OUTPUT_ENABLE_INV);
	gpio_clr(LMS_DATA);
	gpio_clr(LMS_SHIFT_CLOCK);
	gpio_clr(LMS_LATCH_ENABLE_COLS);
	gpio_set(LMS_LATCH_ENABLE_ROWS);
	gpio_make_output(LMS_OUTPUT_ENABLE_INV);
	gpio_make_output(LMS_DATA);
	gpio_make_output(LMS_SHIFT_CLOCK);
	gpio_make_output(LMS_LATCH_ENABLE_COLS);
	gpio_make_output(LMS_LATCH_ENABLE_ROWS);
	gpio_clr(LMS_OUTPUT_ENABLE_INV);

	_lms_configure(0x20, 0x33f3);
}

#define LMDELAY()	{volatile int x; for (int de=0; de<1000; de++) { x+=1; }}

void _lms_shift_byte(uint8_t data)
{
	uint8_t bit;
	for (bit=0; bit<8; bit++)
	{
		gpio_set_or_clr(LMS_DATA, data&1);
		LMDELAY();
		gpio_set(LMS_SHIFT_CLOCK);
		LMDELAY();
		gpio_clr(LMS_SHIFT_CLOCK);
		LMDELAY();
		data=data>>1;
	}
}

#define STROBE(LE)	\
	{ gpio_set(LE); LMDELAY(); gpio_clr(LE); LMDELAY(); }
#define ISTROBE(LE)	\
	{ gpio_clr(LE); LMDELAY(); gpio_set(LE); LMDELAY(); }

void _lms_configure_row(uint8_t rowdata)
{
	_lms_shift_byte(rowdata);
	ISTROBE(LMS_LATCH_ENABLE_ROWS);
}

void _lms_configure_col(uint16_t coldata)
{
	_lms_shift_byte((coldata >> 8) & 0xff);
	_lms_shift_byte((coldata >> 0) & 0xff);
	STROBE(LMS_LATCH_ENABLE_COLS);
}

void _lms_configure(uint8_t rowdata, uint16_t coldata)
{
//	gpio_set(LMS_OUTPUT_ENABLE_INV);
	_lms_configure_row(rowdata);
	_lms_configure_col(coldata);
//	gpio_clr(LMS_OUTPUT_ENABLE_INV);
}
#else //!SIM
void led_matrix_single_init(LEDMatrixSingle *lms) {}
void _lms_configure_row(uint8_t rowdata) {}
void _lms_configure_col(uint16_t coldata) {}

#endif //!SIM
