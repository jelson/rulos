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

#pragma once

/*
 * This file has the defines for the Lectrobox LED Matrix Display
 */

#include <inttypes.h>

#define NUM_COLS 24
#define NUM_ROWS 16
#define NUM_COL_BYTES_2BIT (NUM_COLS/4)
#define NUM_COL_BYTES_8BIT (NUM_COLS)

typedef enum {
	sixmatrix_2bit,
	sixmatrix_8bit
} SixMatrix_mode;

typedef struct {
	uint8_t frameBuffer[NUM_ROWS][NUM_COL_BYTES_8BIT];
	SixMatrix_mode mode;
	uint8_t lastRowPainted;
	uint8_t cycle;
} SixMatrix_Context_t;

void hal_6matrix_init(SixMatrix_Context_t *matrixCtx);
void hal_6matrix_setRow_2bit(SixMatrix_Context_t *matrixCtx, uint8_t *colBytes, uint8_t rowNum);
void hal_6matrix_setRow_8bit(SixMatrix_Context_t *matrixCtx, uint8_t *colBytes, uint8_t rowNum);

