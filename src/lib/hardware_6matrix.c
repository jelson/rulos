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
#include <string.h>

#include "rocket.h"
#include "6matrix.h"
#include "hardware.h"

#define F_CPU 8000000UL
#include <util/delay.h>
#include <avr/interrupt.h>

#define COLLATCH_CLK  GPIO_B0
#define COLLATCH_OE   GPIO_B1
#define COLLATCH_LE   GPIO_B2
#define COLLATCH_SDI  GPIO_B3

#define USEC_PER_ROW 1000

#define DEBUG

static const struct {
	volatile uint8_t *ddr;
	volatile uint8_t *port;
	volatile uint8_t *pin;
	uint8_t bit;
} row_power[] = {
	{ GPIO_D2 }, // 1
	{ GPIO_D1 }, // 2
	{ GPIO_D0 }, // 3
	{ GPIO_C3 }, // 4
	{ GPIO_D5 }, // 5
	{ GPIO_D6 }, // 6
	{ GPIO_D7 }, // 7
	{ GPIO_B4 }, // 8
	{ GPIO_D3 }, // 9
	{ GPIO_D4 }, // 10
	{ GPIO_B6 }, // 11
	{ GPIO_B7 }, // 12
	{ GPIO_C2 }, // 13
	{ GPIO_C1 }, // 14
	{ GPIO_C0 }, // 15
	{ GPIO_B5 }, // 16
};
#define GPIO_ROW(x) (row_power[x].ddr), (row_power[x].port), (row_power[x].pin), (row_power[x].bit)


void shift_row_2bit(uint8_t *colBytes)
{
	// shift in the new data for each column
	for (uint8_t byteNum = 0; byteNum < NUM_COL_BYTES_2BIT; byteNum++, colBytes++) {
		uint8_t currByte = *colBytes;
		for (uint8_t i = 0; i < 8; i++) {
			gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t) 0b10000000);
			currByte <<= 1;
			gpio_set(COLLATCH_CLK);
			gpio_clr(COLLATCH_CLK);
		}
	}
}

void shift_row_2bit_unrolled(uint8_t *colBytes)
{
	// shift in the new data for each column
	for (uint8_t byteNum = 0; byteNum < NUM_COL_BYTES_2BIT; byteNum++, colBytes++) {
		uint8_t currByte = *colBytes;
		gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t) 0b10000000);
		gpio_set(COLLATCH_CLK);
		gpio_clr(COLLATCH_CLK);
		gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t) 0b01000000);
		gpio_set(COLLATCH_CLK);
		gpio_clr(COLLATCH_CLK);
		gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t) 0b00100000);
		gpio_set(COLLATCH_CLK);
		gpio_clr(COLLATCH_CLK);
		gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t) 0b00010000);
		gpio_set(COLLATCH_CLK);
		gpio_clr(COLLATCH_CLK);
		gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t) 0b00001000);
		gpio_set(COLLATCH_CLK);
		gpio_clr(COLLATCH_CLK);
		gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t) 0b00000100);
		gpio_set(COLLATCH_CLK);
		gpio_clr(COLLATCH_CLK);
		gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t) 0b00000010);
		gpio_set(COLLATCH_CLK);
		gpio_clr(COLLATCH_CLK);
		gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t) 0b00000001);
		gpio_set(COLLATCH_CLK);
		gpio_clr(COLLATCH_CLK);
	}
}

void shift_row_8bit(uint8_t *colBytes, uint8_t cycleNum)
{
	for (uint8_t byteNum = 0; byteNum < NUM_COL_BYTES_8BIT; byteNum++, colBytes++) {
		uint8_t tmp = *colBytes;
		// warning - don't tinker with this without ensuring the compiler
		// is still generating 8-bit comparisons.  It really wanted 16-bit comparisons.
		// AVR-GCC faq says bitwise operators promote to 16-bit by default.
		uint8_t tmp2 = tmp;
		tmp2 &= (uint8_t) 0xf0; 
		tmp2 >>= 4;
		if (tmp2 > cycleNum)
			gpio_set(COLLATCH_SDI);
		else
			gpio_clr(COLLATCH_SDI);

		gpio_set(COLLATCH_CLK);
		gpio_clr(COLLATCH_CLK);

		tmp &= (uint8_t) 0x0f; 
		if (tmp > cycleNum)
			gpio_set(COLLATCH_SDI);
		else
			gpio_clr(COLLATCH_SDI);

		gpio_set(COLLATCH_CLK);
		gpio_clr(COLLATCH_CLK);
	}

		
}

void paint_next_row(SixMatrix_Context_t *mat)
{
#ifdef DEBUG
	gpio_set(GPIO_B5);
#endif

	// Using the extra local copies for speed; ptr redirects are slow
	uint8_t prevRow   = mat->lastRowPainted;
	uint8_t currCycle = mat->cycle;
	uint8_t currRow;

	// Compute which row to paint.  If we've reached the end of the row, 
	if (prevRow == NUM_ROWS-1) {
		currRow = 0;

		// Reached the bottom of the display?  Increment the
		// PWM cycle number.
		currCycle++;

		if (currCycle == 16) {
			currCycle = 0;
		}

		mat->cycle = currCycle;
	} else {
		currRow = prevRow+1;
	}

	// shift new row's data into the registers
	gpio_clr(COLLATCH_LE);
	gpio_clr(COLLATCH_CLK);

	if (mat->mode == sixmatrix_2bit) {
		shift_row_2bit_unrolled(mat->frameBuffer[currRow]);
	} else {
		shift_row_8bit(mat->frameBuffer[currRow], currCycle);
	}


	// turn off old row
	gpio_set(COLLATCH_OE);
	gpio_clr(GPIO_ROW(prevRow));

	// assert new row's columns
	gpio_set(COLLATCH_LE);
	gpio_clr(COLLATCH_LE);

	// turn on new row
	gpio_clr(COLLATCH_OE);
	gpio_set(GPIO_ROW(currRow));

	mat->lastRowPainted = currRow;

#ifdef DEBUG
	gpio_clr(GPIO_B5);
#endif
}

void hal_6matrix_setRow_8bit(SixMatrix_Context_t *mat, uint8_t *colBytes, uint8_t rowNum)
{
	memcpy(mat->frameBuffer[rowNum], colBytes, NUM_COL_BYTES_8BIT);
	mat->mode = sixmatrix_8bit;
}

void hal_6matrix_init(SixMatrix_Context_t *mat)
{
	memset(mat, 0, sizeof(*mat));

	gpio_make_output(COLLATCH_CLK);
	gpio_make_output(COLLATCH_OE);
	gpio_make_output(COLLATCH_LE);
	gpio_make_output(COLLATCH_SDI);

	for (uint8_t i = 0; i < NUM_ROWS; i++) {
		gpio_make_output(GPIO_ROW(i));
		gpio_clr(GPIO_ROW(i));
	}
	hal_start_clock_us(USEC_PER_ROW, (Handler) paint_next_row, mat, TIMER1);
}
