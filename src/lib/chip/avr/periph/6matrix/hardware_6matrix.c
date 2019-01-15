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

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/6matrix/6matrix.h"

#define F_CPU 8000000UL
#include <avr/interrupt.h>
#include <util/delay.h>

#define COLLATCH_CLK GPIO_B0
#define COLLATCH_OE GPIO_B1
#define COLLATCH_LE GPIO_B2
#define COLLATCH_SDI GPIO_B3

#define SPI_SHIFTING
//#define TOP_ROW_ONLY

#define DEBUG GPIO_C2

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

#define GPIO_ROW_1 GPIO_D2
#define GPIO_ROW_2 GPIO_D1
#define GPIO_ROW_3 GPIO_D0
#define GPIO_ROW_4 GPIO_C3
#define GPIO_ROW_5 GPIO_D5
#define GPIO_ROW_6 GPIO_D6
#define GPIO_ROW_7 GPIO_D7
#define GPIO_ROW_8 GPIO_B4
#define GPIO_ROW_9 GPIO_D3
#define GPIO_ROW_10 GPIO_D4
#define GPIO_ROW_11 GPIO_B6
#define GPIO_ROW_12 GPIO_B7
#define GPIO_ROW_13 GPIO_C2
#define GPIO_ROW_14 GPIO_C1
#define GPIO_ROW_15 GPIO_C0
#define GPIO_ROW_16 GPIO_B5

typedef void (*funcPtr)();

static inline void row_1_on() { gpio_clr(GPIO_ROW_1); }
static inline void row_2_on() { gpio_clr(GPIO_ROW_2); }
static inline void row_3_on() { gpio_clr(GPIO_ROW_3); }
static inline void row_4_on() { gpio_clr(GPIO_ROW_4); }
static inline void row_5_on() { gpio_clr(GPIO_ROW_5); }
static inline void row_6_on() { gpio_clr(GPIO_ROW_6); }
static inline void row_7_on() { gpio_clr(GPIO_ROW_7); }
static inline void row_8_on() { gpio_clr(GPIO_ROW_8); }
static inline void row_9_on() { gpio_clr(GPIO_ROW_9); }
static inline void row_10_on() { gpio_clr(GPIO_ROW_10); }
static inline void row_11_on() { gpio_clr(GPIO_ROW_11); }
static inline void row_12_on() { gpio_clr(GPIO_ROW_12); }
static inline void row_13_on() { gpio_clr(GPIO_ROW_13); }
static inline void row_14_on() { gpio_clr(GPIO_ROW_14); }
static inline void row_15_on() { /*gpio_clr(GPIO_ROW_15); */
}
static inline void row_16_on() { gpio_clr(GPIO_ROW_16); }

static inline void row_1_off() { gpio_set(GPIO_ROW_1); }
static inline void row_2_off() { gpio_set(GPIO_ROW_2); }
static inline void row_3_off() { gpio_set(GPIO_ROW_3); }
static inline void row_4_off() { gpio_set(GPIO_ROW_4); }
static inline void row_5_off() { gpio_set(GPIO_ROW_5); }
static inline void row_6_off() { gpio_set(GPIO_ROW_6); }
static inline void row_7_off() { gpio_set(GPIO_ROW_7); }
static inline void row_8_off() { gpio_set(GPIO_ROW_8); }
static inline void row_9_off() { gpio_set(GPIO_ROW_9); }
static inline void row_10_off() { gpio_set(GPIO_ROW_10); }
static inline void row_11_off() { gpio_set(GPIO_ROW_11); }
static inline void row_12_off() { gpio_set(GPIO_ROW_12); }
static inline void row_13_off() { gpio_set(GPIO_ROW_13); }
static inline void row_14_off() { gpio_set(GPIO_ROW_14); }
static inline void row_15_off() { /*gpio_set(GPIO_ROW_15);*/
}
static inline void row_16_off() { gpio_set(GPIO_ROW_16); }

const static funcPtr rowOn[] = {row_1_on,  row_2_on,  row_3_on,  row_4_on,
                                row_5_on,  row_6_on,  row_7_on,  row_8_on,
                                row_9_on,  row_10_on, row_11_on, row_12_on,
                                row_13_on, row_14_on, row_15_on, row_16_on};

const static funcPtr rowOff[] = {
    row_1_off,  row_2_off,  row_3_off,  row_4_off,  row_5_off,  row_6_off,
    row_7_off,  row_8_off,  row_9_off,  row_10_off, row_11_off, row_12_off,
    row_13_off, row_14_off, row_15_off, row_16_off};

static inline void gpio_row_power_off(uint8_t rowNum) { rowOff[rowNum](); }

static inline void gpio_row_power_on(uint8_t rowNum) { rowOn[rowNum](); }

#ifdef SPI_SHIFTING

static inline void shift_subframe(uint8_t *colBytes) {
  // Experimented to find the smallest number of NOPs,
  // to avoid the slow-but-safer way of reading SPDR
  // and checking to see if the ready-bit has been cleared.
  // That's necessarily slower because you have to wait
  // for the register to be ready, plus the additional
  // cycles to check, plus you might have been in the middle
  // of a check when the register first became ready.
  SPDR = colBytes[0];
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  SPDR = colBytes[1];
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  SPDR = colBytes[2];
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  SPDR = colBytes[3];
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  SPDR = colBytes[4];
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  _NOP();
  SPDR = colBytes[5];
}

#else
static inline void shift_subframe(uint8_t *colBytes) {
  // shift in the new data for each column
  for (uint8_t byteNum = 0; byteNum < SIXMATRIX_NUM_COL_BYTES_2BIT;
       byteNum++, colBytes++) {
    uint8_t currByte = *colBytes;
    gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t)0b10000000);
    gpio_set(COLLATCH_CLK);
    gpio_clr(COLLATCH_CLK);
    gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t)0b01000000);
    gpio_set(COLLATCH_CLK);
    gpio_clr(COLLATCH_CLK);
    gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t)0b00100000);
    gpio_set(COLLATCH_CLK);
    gpio_clr(COLLATCH_CLK);
    gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t)0b00010000);
    gpio_set(COLLATCH_CLK);
    gpio_clr(COLLATCH_CLK);
    gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t)0b00001000);
    gpio_set(COLLATCH_CLK);
    gpio_clr(COLLATCH_CLK);
    gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t)0b00000100);
    gpio_set(COLLATCH_CLK);
    gpio_clr(COLLATCH_CLK);
    gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t)0b00000010);
    gpio_set(COLLATCH_CLK);
    gpio_clr(COLLATCH_CLK);
    gpio_set_or_clr(COLLATCH_SDI, currByte & (uint8_t)0b00000001);
    gpio_set(COLLATCH_CLK);
    gpio_clr(COLLATCH_CLK);
  }
}
#endif

///// Gamma Correction using Fibonacci Sequences
//
// We'd like to convert the 16 values we get from the user to 16
// visually distinct values, using gamma correction.  We use 8 distinct
// brightness levels in 8 distinct timeslots -- every other number in
// the Fibonacci sequence.  This lets us play a trick, getting twice
// as many distinct levels as we have time slots:
//
// Level 0: No bins
// Level 1: Bin 1
// Level 2: Bin 2
// Level 3: Bin 2+1
// Level 4: Bin 3
// Level 5: Bin 3+2+1
//
// The values of the bins are {1, 3, 8, 21, 55, 144, 377, 987}
//
// We could just map those to times.  However, since the last bin is 987 times
// longer than the first bin, and the first bin can't be smaller than ~50 usec
// (twice the time it takes to shift), that would make the overall update rate
// unacceptably slow.  Therefore, for the smallest bins, we use PWM.
//
// For the last 3 bins, we do extend the time.  We'll use TIMER0 with a /64
// prescaler, so 8 microseconds per tick.  48 is a multiple of 8 so we'll
// normalize all the bins to be portions of 48 microseconds:
// {0.333333, 1., 2.66667, 7., 18.3333, 48., 125.667, 329.}
//
// The last 3 bins are 48, 126, and 329 microseconds, or OCR values
// of {6, 16, 41}.
//
// Earlier bins are PWMed down.  We'll use the 16-bit counter configured with
// no prescaling, with a cycle time of 48 microseconds, so counts up to 384
// (assuming 8 MHz).  Thus, rescaling the first 6 bins as OCR values gives:
// {2.66667, 8., 21.3333, 56., 146.667, 384.,

typedef struct {
  const uint8_t ocr;  // subframe time -- timer0 OCR value, each tick is 8 usec
  const uint16_t
      pwm;  // subframe PWM -- timer1 value, each tick is 1/384th of a frame
} binTime;

static const binTime binTimes[] = {{6, 3},   {6, 8},   {6, 21},   {6, 56},
                                   {6, 147}, {6, 384}, {16, 384}, {41, 384}};

typedef struct {
  uint8_t bin0Val : 1;
  uint8_t bin1Val : 1;
  uint8_t bin2Val : 1;
  uint8_t bin3Val : 1;
  uint8_t bin4Val : 1;
  uint8_t bin5Val : 1;
  uint8_t bin6Val : 1;
  uint8_t bin7Val : 1;
} brightnessToBins;

static const brightnessToBins brightnessesToBins[] = {
    {0, 0, 0, 0, 0, 0, 0, 0},  // brightness 0: no bins
    {1, 0, 0, 0, 0, 0, 0, 0},  // brightness 1: bin 1
    {0, 1, 0, 0, 0, 0, 0, 0},  // brightness 2: bin 2
    {1, 1, 0, 0, 0, 0, 0, 0},  // brightness 3: bin 2+1
    {0, 0, 1, 0, 0, 0, 0, 0},  // brightness 4: bin 3
    {1, 1, 1, 0, 0, 0, 0, 0},  // brightness 5: bin 3+2+1
    {0, 0, 0, 1, 0, 0, 0, 0},  // brightness 6: bin 4
    {1, 1, 1, 1, 0, 0, 0, 0},  // brightness 7: bin 4+3+2+1
    {0, 0, 0, 0, 1, 0, 0, 0},  // ...etc
    {1, 1, 1, 1, 1, 0, 0, 0},  //
    {0, 0, 0, 0, 0, 1, 0, 0},  //
    {1, 1, 1, 1, 1, 1, 0, 0},  //
    {0, 0, 0, 0, 0, 0, 1, 0},  //
    {1, 1, 1, 1, 1, 1, 1, 0},  //
    {0, 0, 0, 0, 0, 0, 0, 1},  //
    {1, 1, 1, 1, 1, 1, 1, 1}};

void hal_6matrix_setRow_8bit(SixMatrix_Context_t *mat, uint8_t *colBytes,
                             uint8_t rowNum) {
#ifdef DEBUG
  gpio_set(DEBUG);
  gpio_clr(DEBUG);
  gpio_set(DEBUG);
#endif
  // The 0-15 brightness values in each of the red and green channels
  // are displayed by keeping the LED on for 0/15ths through 15/15ths of the
  // total time. We display 4 "subframes" for each refresh -- of lengths 1, 2,
  // 4, and 8. Each bit of the brightness value corresponds to whether the LED
  // should be on during that subframe. In this function we construct the 4
  // subframes: (ra1 rb1 rc1 rd1,ga1 gb1 gc1 gd1),(ra2 rb2 rc2 rd2 ,ga2 gb2 gc2
  // gd2)... to (ra1 ga1 ra2 ga2 ra3 ga3...)  (rb1 gb1 rb2 gb2 rb3 gb3...).
  // Here, 1 2 3 4 is the column number, and A B C D is the brightness bit
  // number.
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

  uint8_t outByte0 = 0;
  uint8_t outByte1 = 0;
  uint8_t outByte2 = 0;
  uint8_t outByte3 = 0;
  uint8_t outByte4 = 0;
  uint8_t outByte5 = 0;
  uint8_t outByte6 = 0;
  uint8_t outByte7 = 0;
  uint8_t outCtr = 0;

  int8_t module, col;
  for (module = 2; module >= 0; module--) {
    // left-side columns are green-first
    for (col = 0; col < 4; col++) {
      uint8_t currCol = colBytes[module * 8 + col];
      uint8_t tmp = currCol;
      tmp &= (uint8_t)0x0f;
      brightnessToBins grnBins = brightnessesToBins[tmp];
      currCol >>= 4;
      brightnessToBins redBins = brightnessesToBins[currCol];

      outByte0 <<= 1;
      outByte0 |= grnBins.bin0Val;
      outByte0 <<= 1;
      outByte0 |= redBins.bin0Val;

      outByte1 <<= 1;
      outByte1 |= grnBins.bin1Val;
      outByte1 <<= 1;
      outByte1 |= redBins.bin1Val;

      outByte2 <<= 1;
      outByte2 |= grnBins.bin2Val;
      outByte2 <<= 1;
      outByte2 |= redBins.bin2Val;

      outByte3 <<= 1;
      outByte3 |= grnBins.bin3Val;
      outByte3 <<= 1;
      outByte3 |= redBins.bin3Val;

      outByte4 <<= 1;
      outByte4 |= grnBins.bin4Val;
      outByte4 <<= 1;
      outByte4 |= redBins.bin4Val;

      outByte5 <<= 1;
      outByte5 |= grnBins.bin5Val;
      outByte5 <<= 1;
      outByte5 |= redBins.bin5Val;

      outByte6 <<= 1;
      outByte6 |= grnBins.bin6Val;
      outByte6 <<= 1;
      outByte6 |= redBins.bin6Val;

      outByte7 <<= 1;
      outByte7 |= grnBins.bin7Val;
      outByte7 <<= 1;
      outByte7 |= redBins.bin7Val;
    }

    mat->subframeData[0][rowNum][outCtr] = outByte0;
    mat->subframeData[1][rowNum][outCtr] = outByte1;
    mat->subframeData[2][rowNum][outCtr] = outByte2;
    mat->subframeData[3][rowNum][outCtr] = outByte3;
    mat->subframeData[4][rowNum][outCtr] = outByte4;
    mat->subframeData[5][rowNum][outCtr] = outByte5;
    mat->subframeData[6][rowNum][outCtr] = outByte6;
    mat->subframeData[7][rowNum][outCtr] = outByte7;
    outCtr++;

    // right 4 columns are wired red-first, and MSB-LSB reversed.
    for (col = 7; col >= 4; col--) {
      uint8_t currCol = colBytes[module * 8 + col];
      uint8_t tmp = currCol;
      tmp &= (uint8_t)0x0f;
      brightnessToBins grnBins = brightnessesToBins[tmp];
      currCol >>= 4;
      brightnessToBins redBins = brightnessesToBins[currCol];

      outByte0 <<= 1;
      outByte0 |= redBins.bin0Val;
      outByte0 <<= 1;
      outByte0 |= grnBins.bin0Val;

      outByte1 <<= 1;
      outByte1 |= redBins.bin1Val;
      outByte1 <<= 1;
      outByte1 |= grnBins.bin1Val;

      outByte2 <<= 1;
      outByte2 |= redBins.bin2Val;
      outByte2 <<= 1;
      outByte2 |= grnBins.bin2Val;

      outByte3 <<= 1;
      outByte3 |= redBins.bin3Val;
      outByte3 <<= 1;
      outByte3 |= grnBins.bin3Val;

      outByte4 <<= 1;
      outByte4 |= redBins.bin4Val;
      outByte4 <<= 1;
      outByte4 |= grnBins.bin4Val;

      outByte5 <<= 1;
      outByte5 |= redBins.bin5Val;
      outByte5 <<= 1;
      outByte5 |= grnBins.bin5Val;

      outByte6 <<= 1;
      outByte6 |= redBins.bin6Val;
      outByte6 <<= 1;
      outByte6 |= grnBins.bin6Val;

      outByte7 <<= 1;
      outByte7 |= redBins.bin7Val;
      outByte7 <<= 1;
      outByte7 |= grnBins.bin7Val;
    }

    mat->subframeData[0][rowNum][outCtr] = outByte0;
    mat->subframeData[1][rowNum][outCtr] = outByte1;
    mat->subframeData[2][rowNum][outCtr] = outByte2;
    mat->subframeData[3][rowNum][outCtr] = outByte3;
    mat->subframeData[4][rowNum][outCtr] = outByte4;
    mat->subframeData[5][rowNum][outCtr] = outByte5;
    mat->subframeData[6][rowNum][outCtr] = outByte6;
    mat->subframeData[7][rowNum][outCtr] = outByte7;
    outCtr++;
  }
#ifdef DEBUG
  gpio_clr(DEBUG);
  gpio_set(DEBUG);
  gpio_clr(DEBUG);
#endif
}

void paint_next_row(SixMatrix_Context_t *mat) {
#ifdef DEBUG
  gpio_set(DEBUG);
#endif
  // Using the extra local copies for speed; ptr redirects are slow
  uint8_t prevRow = mat->lastRowPainted;
  uint8_t currSubframe = mat->subframeNum;
  uint8_t currRow;

  // Compute which row to paint.  If we've reached the end of the row,
  // move to the first row of the next subframe.
#ifdef TOP_ROW_ONLY
  if (1)
#else
  if (prevRow == SIXMATRIX_NUM_ROWS - 1)
#endif
  {
    currRow = 0;

    if (currSubframe == SIXMATRIX_NUM_SUBFRAMES - 1) {
      currSubframe = 0;
    } else {
      currSubframe++;
    }
    // reconfigure the time we spend on each subframe
    OCR0A = binTimes[currSubframe].ocr;  // set the time until next frame
    mat->subframeNum = currSubframe;
  } else {
    currRow = prevRow + 1;
  }

  // shift new row's data into the registers
  shift_subframe(mat->subframeData[currSubframe][currRow]);

  // do other book-keeping now, to give last byte time to shift out
  mat->lastRowPainted = currRow;

  // turn off old row
  gpio_row_power_off(prevRow);
  OCR1A = 0;
  TCNT1 = 383;

  // assert new row's columns
  gpio_set(COLLATCH_LE);
  gpio_clr(COLLATCH_LE);

  OCR1A = binTimes[currSubframe].pwm;  // set PWM of this subframe
  TCNT1 = 300;                         // just before the flip time

  // turn on power to the new row
  gpio_row_power_on(currRow);

#ifdef DEBUG
  gpio_clr(DEBUG);
#endif
}

void hal_6matrix_init(SixMatrix_Context_t *mat) {
  memset(mat, 0, sizeof(*mat));

  // spi bluewire
#ifdef SPI_SHIFTING
  gpio_make_input_disable_pullup(COLLATCH_CLK);
#else
  gpio_make_output(COLLATCH_CLK);
#endif

  gpio_make_output(COLLATCH_OE);
  gpio_make_output(COLLATCH_LE);
  gpio_make_output(COLLATCH_SDI);

#ifdef SPI_SHIFTING
  gpio_make_output(GPIO_B2);  // SS
  gpio_make_output(GPIO_B3);  // MOSI
  gpio_make_output(GPIO_B5);  // SCK
  SPCR = _BV(SPE) |           // SPI enable
                              // Interrupts not enabled
                              //_BV(DORD) |  // Transmit MSB first
         _BV(MSTR);           // Master mode
  // CPOL = 0, CPHA = 0 -- bit must be stable during clock rising edge
  // SPR1 =0 0, SPR0 = 0 -- Fosc/2

  SPSR = _BV(SPI2X);  // Fosc/2
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

  // Use Timer0 for paint clock
  hal_start_clock_us(329, (Handler)paint_next_row, mat, TIMER0);

  // Set up PWM on Timer1
  TCCR1A = 0 | _BV(COM1A1) | _BV(COM1A0)  // inverting PWM on OC1A
           | _BV(WGM11)                   // Fast PWM, 16-bit, TOP=ICR1
      ;
  TCCR1B = 0 | _BV(WGM13)  // Fast PWM, 16-bit, TOP=ICR1
           | _BV(WGM12)    // Fast PWM, 16-bit, TOP=ICR1
           | _BV(CS10)     // no prescaler => 1/8 us per tick
      ;

  ICR1 = 384;
  OCR1A = 0;
}
