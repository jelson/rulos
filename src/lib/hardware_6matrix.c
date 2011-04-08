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
#define SPI_SHIFTING

#define DEBUG GPIO_D4


/////  Turning row transistors on and off quickly /////

// I spent quite a lot of time tinkering with various methods for
// passing a row number (integer) as a parameter and turning that
// row number on and off quickly.  Some early attempts:
//
// 1) an array of GPIO_D2-type constants, that could be indexed
// at run time and passed to gpio_set and gpio_clr.  Took a long
// time (~10 usec) because gpio_set and gpio_clr were no longer
// capable of being inlined to a single statement, and the 
// macro that accessed it indexed into the array 4 times.
//
// 2) similar to the above, but copying the array entry into
// a local struct, then calling gpio.  Array now indexed only
// once, but inlining problem remains.
// 
// 3) a 16-way switch statement in which easy case was a
// gpio_set or gpio_clr parameterized with a constant.
// Allowed gpio_set/clr to be inlined again, but the switch
// statement was dozens of instructions and the compiler wasn't
// smart enough to build a binary search tree.
//
// 4) The final method I settled on -- an array of function pointers
// to one-statement gpio_set or gpio_clr functions.  This allows
// inlining, and requires indexing once into an array to find a
// function pointer, and jumping to it.  Also means that each row
// has constant-time access; with the switch it was variable-time.
//
// Also, I originally used gpio_set_or_clr, but since we always pass
// a constant (there's one place in the code where we turn rows on
// and another where we turn them off), we can save the comparisons
// by making one set of set and one set of clr functions.
//
// Put this all together, and the parameterized set/clr only takes 1.25
// microseconds -- 10 instructions.
//

#define GPIO_ROW_1  GPIO_D2
#define GPIO_ROW_2  GPIO_D1
#define GPIO_ROW_3  GPIO_D0
#define GPIO_ROW_4  GPIO_C3
#define GPIO_ROW_5  GPIO_D5
#define GPIO_ROW_6  GPIO_D6
#define GPIO_ROW_7  GPIO_D7
#define GPIO_ROW_8  GPIO_B4
#define GPIO_ROW_9  GPIO_D3
#define GPIO_ROW_10 GPIO_D4
#define GPIO_ROW_11 GPIO_B6
#define GPIO_ROW_12 GPIO_B7
#define GPIO_ROW_13 GPIO_C2
#define GPIO_ROW_14 GPIO_C1
#define GPIO_ROW_15 GPIO_C0
#define GPIO_ROW_16 GPIO_B5

typedef void (*funcPtr)();

static inline void row_1_off()  { gpio_clr(GPIO_ROW_1); }
static inline void row_2_off()  { gpio_clr(GPIO_ROW_2); }
static inline void row_3_off()  { gpio_clr(GPIO_ROW_3); }
static inline void row_4_off()  { gpio_clr(GPIO_ROW_4); }
static inline void row_5_off()  { gpio_clr(GPIO_ROW_5); }
static inline void row_6_off()  { gpio_clr(GPIO_ROW_6); }
static inline void row_7_off()  { gpio_clr(GPIO_ROW_7); }
static inline void row_8_off()  { gpio_clr(GPIO_ROW_8); }
static inline void row_9_off()  { gpio_clr(GPIO_ROW_9); }
static inline void row_10_off() { gpio_clr(GPIO_ROW_10); }
static inline void row_11_off() { gpio_clr(GPIO_ROW_11); }
static inline void row_12_off() { gpio_clr(GPIO_ROW_12); }
static inline void row_13_off() { gpio_clr(GPIO_ROW_13); }
static inline void row_14_off() { gpio_clr(GPIO_ROW_14); }
static inline void row_15_off() { gpio_clr(GPIO_ROW_15); }
static inline void row_16_off() { gpio_clr(GPIO_ROW_16); }

static inline void row_1_on()  { gpio_set(GPIO_ROW_1); }
static inline void row_2_on()  { gpio_set(GPIO_ROW_2); }
static inline void row_3_on()  { gpio_set(GPIO_ROW_3); }
static inline void row_4_on()  { gpio_set(GPIO_ROW_4); }
static inline void row_5_on()  { gpio_set(GPIO_ROW_5); }
static inline void row_6_on()  { gpio_set(GPIO_ROW_6); }
static inline void row_7_on()  { gpio_set(GPIO_ROW_7); }
static inline void row_8_on()  { gpio_set(GPIO_ROW_8); }
static inline void row_9_on()  { gpio_set(GPIO_ROW_9); }
static inline void row_10_on() { gpio_set(GPIO_ROW_10); }
static inline void row_11_on() { gpio_set(GPIO_ROW_11); }
static inline void row_12_on() { gpio_set(GPIO_ROW_12); }
static inline void row_13_on() { gpio_set(GPIO_ROW_13); }
static inline void row_14_on() { gpio_set(GPIO_ROW_14); }
static inline void row_15_on() { gpio_set(GPIO_ROW_15); }
static inline void row_16_on() { gpio_set(GPIO_ROW_16); }

const static funcPtr rowOff[] = {
	row_1_off,
	row_2_off,
	row_3_off,
	row_4_off,
	row_5_off,
	row_6_off,
	row_7_off,
	row_8_off,
	row_9_off,
	row_10_off,
	row_11_off,
	row_12_off,
	row_13_off,
	row_14_off,
	row_15_off,
	row_16_off
};

const static funcPtr rowOn[] = {
	row_1_on,
	row_2_on,
	row_3_on,
	row_4_on,
	row_5_on,
	row_6_on,
	row_7_on,
	row_8_on,
	row_9_on,
	row_10_on,
	row_11_on,
	row_12_on,
	row_13_on,
	row_14_on,
	row_15_on,
	row_16_on
};

static inline void gpio_row_power_off(uint8_t rowNum)
{
	rowOff[rowNum]();
}

static inline void gpio_row_power_on(uint8_t rowNum)
{
	rowOn[rowNum]();
}

void old_gpio_row_power_on(uint8_t rowNum)
{
	switch (rowNum) {
	case  0: gpio_set(GPIO_D2); break;
	case  1: gpio_set(GPIO_D1); break;
	case  2: gpio_set(GPIO_D0); break;
	case  3: gpio_set(GPIO_C3); break;
	case  4: gpio_set(GPIO_D5); break;
	case  5: gpio_set(GPIO_D6); break;
	case  6: gpio_set(GPIO_D7); break;
	case  7: gpio_set(GPIO_B4); break;
	case  8: gpio_set(GPIO_D3); break;
	case  9: gpio_set(GPIO_D4); break;
	case 10: gpio_set(GPIO_B6); break;
	case 11: gpio_set(GPIO_B7); break;
	case 12: gpio_set(GPIO_C2); break;
	case 13: gpio_set(GPIO_C1); break;
	case 14: gpio_set(GPIO_C0); break;
	case 15: break; // spi bluewire gpio_set(GPIO_B5); break;
	}
}

void old_gpio_row_power_off(uint8_t rowNum)
{
	switch (rowNum) {
	case  0: gpio_clr(GPIO_D2); break;
	case  1: gpio_clr(GPIO_D1); break;
	case  2: gpio_clr(GPIO_D0); break;
	case  3: gpio_clr(GPIO_C3); break;
	case  4: gpio_clr(GPIO_D5); break;
	case  5: gpio_clr(GPIO_D6); break;
	case  6: gpio_clr(GPIO_D7); break;
	case  7: gpio_clr(GPIO_B4); break;
	case  8: gpio_clr(GPIO_D3); break;
	case  9: gpio_clr(GPIO_D4); break;
	case 10: gpio_clr(GPIO_B6); break;
	case 11: gpio_clr(GPIO_B7); break;
	case 12: gpio_clr(GPIO_C2); break;
	case 13: gpio_clr(GPIO_C1); break;
	case 14: gpio_clr(GPIO_C0); break;
	case 15: break; // spi bluewire gpio_clr(GPIO_B5); break;
	}
}

#ifdef SPI_SHIFTING

static inline void shift_subframe(uint8_t *colBytes)
{
	// Experimented to find the smallest number of NOPs,
	// to avoid the slow-but-safer way of reading SPDR
	// and checking to see if the ready-bit has been cleared.
	// That's necessarily slower because you have to wait
	// for the register to be ready, plus the additional
	// cycles to check, plus you might have been in the middle
	// of a check when the register first became ready.
	SPDR = colBytes[0];
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP();
	SPDR = colBytes[1];
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP();
	SPDR = colBytes[2];
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP();
	SPDR = colBytes[3];
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP();
	SPDR = colBytes[4];
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP(); _NOP();  _NOP(); 
	_NOP(); _NOP();
	SPDR = colBytes[5];
}

#else
static inline void shift_subframe(uint8_t *colBytes)
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
#endif


void paint_next_row(SixMatrix_Context_t *mat)
{
#ifdef DEBUG
	gpio_set(DEBUG);
#endif
	// Using the extra local copies for speed; ptr redirects are slow
	uint8_t prevRow      = mat->lastRowPainted;
	uint8_t currSubframe = mat->subframeNum;
	uint8_t currRow;

	// Compute which row to paint.  If we've reached the end of the row, 
	// move to the first row of the next subframe.
	if (prevRow == SIXMATRIX_NUM_ROWS-1) {
		currRow = 0;

		if (currSubframe == SIXMATRIX_BITS_PER_COL-1) {
			currSubframe = 0;
		} else {
			currSubframe++;
		}
		// reconfigure the time we spend on each subframe
		OCR1A = mat->subframeOcr[currSubframe];
		mat->subframeNum = currSubframe;
	} else {
		currRow = prevRow+1;
	}

	// shift new row's data into the registers
	shift_subframe(mat->subframeData[currSubframe][currRow]);

	// do other book-keeping now, to give last byte time to shift out
	mat->lastRowPainted = currRow;

	// turn off old row
	gpio_set(COLLATCH_OE);
	gpio_row_power_off(prevRow);

	// assert new row's columns
	gpio_set(COLLATCH_LE);
	gpio_clr(COLLATCH_LE);

	// turn on new row
	gpio_clr(COLLATCH_OE);
	gpio_row_power_on(currRow);

#ifdef DEBUG
	gpio_clr(DEBUG);
#endif
}

typedef struct {
	uint8_t bin1Val:1;
	uint8_t bin2Val:1;
	uint8_t bin3Val:1;
	uint8_t bin4Val:1;
	uint8_t bin5Val:1;
	uint8_t bin6Val:1;
	uint8_t bin7Val:1;
	uint8_t bin8Val:1;
} binSpec;

static const binSpec binSpecs[] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0 }, // brightness 0: no bins
	{ 1, 0, 0, 0, 0, 0, 0, 0 }, // brightness 1: bin 1 only
	{ 0, 1, 0, 0, 0, 0, 0, 0 }, // brightness 2: bin 2 only
	{ 1, 1, 0, 0, 0, 0, 0, 0 }, // brightness 3: bin 1 and 2
	{ 0, 0, 1, 0, 0, 0, 0, 0 }, // 4
	{ 1, 1, 1, 0, 0, 0, 0, 0 }, // 5
	{ 0, 0, 0, 1, 0, 0, 0, 0 }, // 
	{ 1, 1, 1, 1, 0, 0, 0, 0 }, // 
	{ 0, 0, 0, 0, 1, 0, 0, 0 }, // 
	{ 1, 1, 1, 1, 1, 0, 0, 0 }, // 
	{ 0, 0, 0, 0, 0, 1, 0, 0 }, // 
	{ 1, 1, 1, 1, 1, 1, 0, 0 }, // 
	{ 0, 0, 0, 0, 0, 0, 1, 0 }, // 
	{ 1, 1, 1, 1, 1, 1, 1, 0 }, // 
	{ 0, 0, 0, 0, 0, 0, 0, 1 }, // 
	{ 1, 1, 1, 1, 1, 1, 1, 1 }
};


void hal_6matrix_setRow_8bit(SixMatrix_Context_t *mat, uint8_t *colBytes, uint8_t rowNum)
{
#ifdef DEBUG
	gpio_set(DEBUG);
	gpio_clr(DEBUG);
	gpio_set(DEBUG);
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
	uint8_t outByte3 = 0;
	uint8_t outByte4 = 0;
	uint8_t outByte5 = 0;
	uint8_t outByte6 = 0;
	uint8_t outByte7 = 0;
	uint8_t outByte8 = 0;
	uint8_t outCtr = 0;

	int8_t module, col;
	for (module = 2; module >= 0; module--) {

		// left-side columns are green-first
		for (col = 0; col < 4; col++) {
			uint8_t currCol = colBytes[module*8 + col];
			uint8_t tmp = currCol;
			tmp &= (uint8_t) 0x0f;
			binSpec b = binSpecs[tmp];

			outByte1 |= b.bin1Val;
			outByte2 |= b.bin2Val;
			outByte3 |= b.bin3Val;
			outByte4 |= b.bin4Val;
			outByte5 |= b.bin5Val;
			outByte6 |= b.bin6Val;
			outByte7 |= b.bin7Val;
			outByte8 |= b.bin8Val;

#if 0
			switch (tmp) {
			case 1: 
				outBin1 |= 1;
				break;

			case 2:
				outBin2 |= 1;
				break;
			case 3:
				outBin2 |= 1;
				outBin1 |= 1;
				break;

			case 4:
				outBin3 |= 1;
				break;
			case 5:
				outBin3 |= 1;
				outBin2 |= 1;
				outBin1 |= 1;
				break;

			case 6:
				outBin4 |= 1;
				break;
			case 7:
				outBin4 |= 1;
				outBin3 |= 1;
				outBin2 |= 1;
				outBin1 |= 1;
				break;

			case 8:
				outBin5 |= 1;
				break;
			case 9:
				outBin5 |= 1;
				outBin4 |= 1;
				outBin3 |= 1;
				outBin2 |= 1;
				outBin1 |= 1;
				break;

			case 10:
				outBin6 |= 1;
				break;
			case 11:
				outBin6 |= 1;
				outBin5 |= 1;
				outBin4 |= 1;
				outBin3 |= 1;
				outBin2 |= 1;
				outBin1 |= 1;
				break;

			case 12:
				outBin7 |= 1;
				break;
			case 13:
				outBin7 |= 1;
				outBin6 |= 1;
				outBin5 |= 1;
				outBin4 |= 1;
				outBin3 |= 1;
				outBin2 |= 1;
				outBin1 |= 1;
				break;

			case 14:
				outBin8 |= 1;
				break;
			case 15:
				outBin8 |= 1;
				outBin7 |= 1;
				outBin6 |= 1;
				outBin5 |= 1;
				outBin4 |= 1;
				outBin3 |= 1;
				outBin2 |= 1;
				outBin1 |= 1;
				break;
			}
#endif

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
	gpio_clr(DEBUG);
	gpio_set(DEBUG);
	gpio_clr(DEBUG);
#endif
}

void hal_6matrix_init(SixMatrix_Context_t *mat)
{
	memset(mat, 0, sizeof(*mat));

	// spi bluewire
	//gpio_make_output(COLLATCH_CLK);
	gpio_make_input(COLLATCH_CLK);

	gpio_make_output(COLLATCH_OE);
	gpio_make_output(COLLATCH_LE);
	gpio_make_output(COLLATCH_SDI);

#ifdef SPI_SHIFTING
	gpio_make_output(GPIO_B2); // SS
	gpio_make_output(GPIO_B3); // MOSI
	gpio_make_output(GPIO_B5); // SCK
	SPCR = 
		_BV(SPE)  |  // SPI enable
		// Interrupts not enabled
		//_BV(DORD) |  // Transmit MSB first
		_BV(MSTR) ;  // Master mode
		// CPOL = 0, CPHA = 0 -- bit must be stable during clock rising edge
		// SPR1 =0 0, SPR0 = 0 -- Fosc/2

	SPSR = _BV(SPI2X); // Fosc/2
#endif

	gpio_make_output(GPIO_ROW_1);
	gpio_make_output(GPIO_ROW_2);
	gpio_make_output(GPIO_ROW_3);
	gpio_make_output(GPIO_ROW_4);
	gpio_make_output(GPIO_ROW_5);
	gpio_make_output(GPIO_ROW_6);
	gpio_make_output(GPIO_ROW_7);
	gpio_make_output(GPIO_ROW_8);
	gpio_make_output(GPIO_ROW_9);
	gpio_make_output(GPIO_ROW_10);
	gpio_make_output(GPIO_ROW_11);
	gpio_make_output(GPIO_ROW_12);
	gpio_make_output(GPIO_ROW_13);
	gpio_make_output(GPIO_ROW_14);
	gpio_make_output(GPIO_ROW_15);

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

