/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "periph/6matrix/6matrix.h"

#include "core/rulos.h"

typedef struct {
  int cycle;
  SixMatrix_Context_t matrix;
} drawCtx;

void makeBarGraph_2bit(uint8_t numCols, uint8_t *output) {
  uint8_t *currPtr = output;

  memset(output, 0, SIXMATRIX_NUM_COL_BYTES_2BIT);

  while (numCols >= 8) {
    *currPtr = 0xff;
    currPtr++;
    numCols -= 8;
  }

  // for the last one, shift in as many ones as there are columns
  uint8_t i;
  for (i = 0; i < numCols; i++) {
    *currPtr |= ((0b10000000) >> i);
  }
}

#define MAKE_COLOR(r, g) (((r & 0b1111) << 4) | (g & 0b1111))

void update_matrix(drawCtx *draw) {
  uint8_t colBytes[SIXMATRIX_NUM_COLS];
  uint8_t colNum, rowNum;
  uint8_t red, green;

  schedule_us(100000, (ActivationFuncPtr)update_matrix, draw);

  for (rowNum = 0, red = draw->cycle; rowNum < 16; rowNum++, red++) {
    if (red >= 16) red = 0;

    for (colNum = 0, green = draw->cycle; colNum < SIXMATRIX_NUM_COLS;
         colNum++, green++) {
      if (green >= 16) green = 0;
      colBytes[colNum] = MAKE_COLOR(red, green);
    }
    hal_6matrix_setRow_8bit(&(draw->matrix), colBytes, rowNum);
  }
  draw->cycle++;

  if (draw->cycle > 16) draw->cycle = 0;
}

void one_gradient_row(drawCtx *draw) {
  uint8_t bytes[SIXMATRIX_NUM_COLS];
  uint8_t i;
  uint8_t color = 0;
  for (i = 0; i < SIXMATRIX_NUM_COLS; i++) {
    bytes[i] = MAKE_COLOR(color, 0);

    if (color < 15) color++;
  }
  hal_6matrix_setRow_8bit(&(draw->matrix), bytes, 0);
  // schedule_us(1000000, (ActivationFuncPtr) one_gradient_row, draw);
}

void one_led(drawCtx *draw) {
  uint8_t bytes[SIXMATRIX_NUM_COLS];
  uint8_t i;
  uint8_t color = 0;

  schedule_us(2500000, (ActivationFuncPtr)one_led, draw);

  if (draw->cycle < SIXMATRIX_NUM_COLS)
    color = MAKE_COLOR(0xf, 0);
  else
    color = MAKE_COLOR(0, 0xf);

  for (i = 0; i < SIXMATRIX_NUM_COLS; i++) {
    if (i == draw->cycle || i + SIXMATRIX_NUM_COLS == draw->cycle)
      bytes[i] = color;
    else
      bytes[i] = 0;
  }

  hal_6matrix_setRow_8bit(&(draw->matrix), bytes, 0);
  if (++draw->cycle == 2 * SIXMATRIX_NUM_COLS) draw->cycle = 0;
}

void allon(drawCtx *draw) {
  uint8_t bytes[SIXMATRIX_NUM_COLS];
  uint8_t i;

  memset(bytes, 0xff, sizeof(bytes));
  for (i = 0; i < 16; i++) hal_6matrix_setRow_8bit(&draw->matrix, bytes, i);
}

void rowtest(drawCtx *draw) {
  uint8_t bytes[SIXMATRIX_NUM_COLS];
  uint8_t i;

  memset(bytes, 0, sizeof(bytes));
  for (i = 0; i < 8; i++) bytes[i] = 0xf;
  hal_6matrix_setRow_8bit(&draw->matrix, bytes, 0);

  memset(bytes, 0, sizeof(bytes));
  for (i = 8; i < 16; i++) bytes[i] = 0xf;
  hal_6matrix_setRow_8bit(&draw->matrix, bytes, 4);

  memset(bytes, 0, sizeof(bytes));
  for (i = 16; i < 24; i++) bytes[i] = 0xf;
  hal_6matrix_setRow_8bit(&draw->matrix, bytes, 5);
}

drawCtx draw;

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER2);

  hal_6matrix_init(&draw.matrix);

  draw.cycle = 0;
  // schedule_us(1, (ActivationFuncPtr) one_gradient_row, &draw);
  // schedule_us(1, (ActivationFuncPtr) update_matrix, &draw);
  // schedule_us(1, (ActivationFuncPtr) one_led, &draw);
  rowtest(&draw);

  scheduler_run();
  assert(FALSE);

#if 0
	uint8_t numCols = 0;
	while (1) {
		makeBarGraph(numCols, testPattern);
		hal_6matrix_setRow(testPattern, 0);
		_delay_ms(350);
		numCols++;
		if (numCols > NUM_COLS)
			numCols = 0;
	}
#endif

#if 0
	uint8_t rowNum = 0, lastRowNum = 0;
	while (1) {
		row_power(lastRowNum, 0);
		row_power(rowNum, 1);
		lastRowNum = rowNum;
		rowNum = (rowNum + 1) % NUM_ROWS;
		_delay_ms(1000);
	}
#endif
}
