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

#pragma once

/*
 * This file has the defines for the Lectrobox LED Matrix Display
 */

#include <inttypes.h>

#define SIXMATRIX_NUM_COLS 24
#define SIXMATRIX_NUM_ROWS 16
#define SIXMATRIX_NUM_SUBFRAMES 8

#define SIXMATRIX_NUM_COL_BYTES_2BIT (SIXMATRIX_NUM_COLS / 4)
#define SIXMATRIX_NUM_COL_BYTES_8BIT (SIXMATRIX_NUM_COLS)

typedef enum { sixmatrix_2bit, sixmatrix_8bit } SixMatrix_mode;

typedef struct {
  uint16_t subframeOcr[SIXMATRIX_NUM_SUBFRAMES];
  uint8_t subframeData[SIXMATRIX_NUM_SUBFRAMES][SIXMATRIX_NUM_ROWS]
                      [SIXMATRIX_NUM_COL_BYTES_2BIT];
  uint8_t subframeNum;
  SixMatrix_mode mode;
  uint8_t lastRowPainted;
} SixMatrix_Context_t;

void hal_6matrix_init(SixMatrix_Context_t *matrixCtx);
void hal_6matrix_setRow_2bit(SixMatrix_Context_t *matrixCtx, uint8_t *colBytes,
                             uint8_t rowNum);
void hal_6matrix_setRow_8bit(SixMatrix_Context_t *matrixCtx, uint8_t *colBytes,
                             uint8_t rowNum);
