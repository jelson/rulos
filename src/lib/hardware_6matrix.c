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

#define SHORTEST_SUBFRAME_USEC 85

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

void gpio_row_power(uint8_t rowNum, uint8_t onoff)
{
	switch (rowNum) {
	case  0: gpio_set_or_clr(GPIO_D2, onoff); break;
	case  1: gpio_set_or_clr(GPIO_D1, onoff); break;
	case  2: gpio_set_or_clr(GPIO_D0, onoff); break;
	case  3: gpio_set_or_clr(GPIO_C3, onoff); break;
	case  4: gpio_set_or_clr(GPIO_D5, onoff); break;
	case  5: gpio_set_or_clr(GPIO_D6, onoff); break;
	case  6: gpio_set_or_clr(GPIO_D7, onoff); break;
	case  7: gpio_set_or_clr(GPIO_B4, onoff); break;
	case  8: gpio_set_or_clr(GPIO_D3, onoff); break;
	case  9: gpio_set_or_clr(GPIO_D4, onoff); break;
	case 10: gpio_set_or_clr(GPIO_B6, onoff); break;
	case 11: gpio_set_or_clr(GPIO_B7, onoff); break;
	case 12: gpio_set_or_clr(GPIO_C2, onoff); break;
	case 13: gpio_set_or_clr(GPIO_C1, onoff); break;
	case 14: gpio_set_or_clr(GPIO_C0, onoff); break;
	case 15: gpio_set_or_clr(GPIO_B5, onoff); break;
	}
}



void shift_subframe(uint8_t *colBytes)
{
	// shift in the new data for each column
	for (uint8_t byteNum = 0; byteNum < SIXMATRIX_NUM_COL_BYTES_2BIT; byteNum++, colBytes++) {
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


void paint_next_row(SixMatrix_Context_t *mat)
{
#ifdef DEBUG
	gpio_set(GPIO_B5);
#endif

	// Using the extra local copies for speed; ptr redirects are slow
	uint8_t prevRow      = mat->lastRowPainted;
	uint8_t currSubframe = mat->subframeNum;
	uint8_t currRow;

	// Compute which row to paint.  If we've reached the end of the row, 
	// move to the first row of the next subframe.
	if (prevRow == SIXMATRIX_NUM_ROWS-1) {
		//if (1) {
		currRow = 0;

		if (currSubframe == SIXMATRIX_BITS_PER_COL-1) {
			currSubframe = 0;
		} else {
			currSubframe++;
		}
		
		mat->subframeNum = currSubframe;

		// reconfigure the time we spend on each subframe
		OCR1A = mat->subframeOcr[currSubframe];

	} else {
		currRow = prevRow+1;
	}

	// shift new row's data into the registers
	gpio_clr(COLLATCH_LE);
	gpio_clr(COLLATCH_CLK);
	shift_subframe(mat->subframeData[currSubframe][currRow]);

	// turn off old row
	gpio_set(COLLATCH_OE);
	gpio_row_power(prevRow, 0);

	// assert new row's columns
	gpio_set(COLLATCH_LE);
	gpio_clr(COLLATCH_LE);

	// turn on new row
	gpio_clr(COLLATCH_OE);
	gpio_row_power(currRow, 1);

	mat->lastRowPainted = currRow;

#ifdef DEBUG
	gpio_clr(GPIO_B5);
#endif
}

void hal_6matrix_setRow_8bit(SixMatrix_Context_t *mat, uint8_t *colBytes, uint8_t rowNum)
{
#ifdef DEBUG
	gpio_set(GPIO_B5);
	gpio_clr(GPIO_B5);
	gpio_set(GPIO_B5);
#endif
	// The 0-15 brightness values in each of the red and green channels
	// are displayed by keeping the LED on for 0/15ths through 15/15ths of the total time.
	// We display 4 "subframes" for each refresh -- of lengths 1, 2, 4, and 8.
	// Each bit of the brightness value corresponds to whether the LED should be on
	// during that subframe.
	// In this function we construct the 4 subframes:
	// (ra1 rb1 rc1 rd1,ga1 gb1 gc1 gd1),(ra2 rb2 rc2 rd2 ,ga2 gb2 gc2 gd2)... to
	// (ra1 ga1 ra2 ga2 ra3 ga3...)  (rb1 gb1 rb2 gb2 rb3 gb3...).
	// Here, 1 2 3 4 is the column number, and A B C D is the brightness bit number.
	//
	// We are constructing the bitstream that is to be shifted in, and the MSB of 
	// what we construct ends up being the LSB on the hardware (since we read the
	// shifted bytes left-to-right), so we also reverse the order of the modules.
	//
	// HARDWARE DESCRIPTION (pcb 1.x):
	//   1st shifted bit is the left-most  green LED of the right-most module.
	//   2nd                               red
	//   3rd                    second     green
	//   4th                               red
	// [...]
	//   9th                    right-most red
	//   10th                              green
	//   11th                   7th        red
	//   12th                              green
	// [...]
	//   17th                   left-most  green            middle

	uint8_t outByte1 = 0;
	uint8_t outByte2 = 0;
	uint8_t outByte4 = 0;
	uint8_t outByte8 = 0;
	uint8_t outCtr = 0;

	int8_t module, col;
	for (module = 2; module >= 0; module--) {

		// left-side columns are green-first
		for (col = 0; col < 4; col++) {
			uint8_t tmp = colBytes[module*8 + col];

			outByte8 <<= 1;
			if (tmp & 0b00001000) // green bit 4
				outByte8 |= 1;

			outByte8 <<= 1;
			if (tmp & 0b10000000) // red bit 4
				outByte8 |= 1;

		
			outByte4 <<= 1;
			if (tmp & 0b00000100) // green bit 3
				outByte4 |= 1;

			outByte4 <<= 1;
			if (tmp & 0b01000000) // red bit 3
				outByte4 |= 1;


			outByte2 <<= 1;
			if (tmp & 0b00000010) // green bit 2
				outByte2 |= 1;

			outByte2 <<= 1;
			if (tmp & 0b00100000) // red bit 2
				outByte2 |= 1;


			outByte1 <<= 1;
			if (tmp & 0b00000001) // green bit 1
				outByte1 |= 1;

			outByte1 <<= 1;
			if (tmp & 0b00010000) // red bit 1
				outByte1 |= 1;
		}

		mat->subframeData[0][rowNum][outCtr] = outByte1;
		mat->subframeData[1][rowNum][outCtr] = outByte2;
		mat->subframeData[2][rowNum][outCtr] = outByte4;
		mat->subframeData[3][rowNum][outCtr] = outByte8;
		outCtr++;

		// right 4 columns are wired red-first, and MSB-LSB reversed.
		for (col = 7; col >= 4; col--) {
			uint8_t tmp = colBytes[module*8 + col];

			outByte8 <<= 1;
			if (tmp & 0b10000000) // red bit 4
				outByte8 |= 1;

			outByte8 <<= 1;
			if (tmp & 0b00001000) // green bit 4
				outByte8 |= 1;

			
			outByte4 <<= 1;
			if (tmp & 0b01000000) // red bit 3
				outByte4 |= 1;

			outByte4 <<= 1;
			if (tmp & 0b00000100) // green bit 3
				outByte4 |= 1;


			outByte2 <<= 1;
			if (tmp & 0b00100000) // red bit 2
				outByte2 |= 1;

			outByte2 <<= 1;
			if (tmp & 0b00000010) // green bit 2
				outByte2 |= 1;


			outByte1 <<= 1;
			if (tmp & 0b00010000) // red bit 1
				outByte1 |= 1;

			outByte1 <<= 1;
			if (tmp & 0b00000001) // green bit 1
				outByte1 |= 1;
		}

		mat->subframeData[0][rowNum][outCtr] = outByte1;
		mat->subframeData[1][rowNum][outCtr] = outByte2;
		mat->subframeData[2][rowNum][outCtr] = outByte4;
		mat->subframeData[3][rowNum][outCtr] = outByte8;
		outCtr++;
	}
#ifdef DEBUG
	gpio_clr(GPIO_B5);
	gpio_set(GPIO_B5);
	gpio_clr(GPIO_B5);
#endif
}

void hal_6matrix_init(SixMatrix_Context_t *mat)
{
	memset(mat, 0, sizeof(*mat));

	gpio_make_output(COLLATCH_CLK);
	gpio_make_output(COLLATCH_OE);
	gpio_make_output(COLLATCH_LE);
	gpio_make_output(COLLATCH_SDI);

	for (uint8_t i = 0; i < SIXMATRIX_NUM_ROWS; i++) {
		gpio_make_output(GPIO_ROW(i));
		gpio_clr(GPIO_ROW(i));
	}
	hal_start_clock_us(SHORTEST_SUBFRAME_USEC*8, (Handler) paint_next_row, mat, TIMER1);

	// hal_start_clock_us finds the correct prescale value and
	// counter value for the longest subframe -- 8 times the
	// shortest, assuming 4 bits per frame.  Now, compute
	// the OCR value for the shorter subframes.
	uint16_t ocr = OCR1A;
	mat->subframeOcr[3] = ocr;  ocr >>= 1;
	mat->subframeOcr[2] = ocr;  ocr >>= 1;
	mat->subframeOcr[1] = ocr;  ocr >>= 1;
	mat->subframeOcr[0] = ocr;
}

