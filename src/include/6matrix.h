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

/*
 * This file has the defines for the Lectrobox LED Matrix Display
 */

#include <inttypes.h>

#define NUM_COLUMNS 48
#define NUM_COLUMN_BYTES_1BIT (NUM_COLUMNS/8)
#define NUM_COLUMN_BYTES_4BIT (NUM_COLUMNS/2)
#define NUM_ROWS    16

void hal_6matrix_init();
void hal_6matrix_setRow(uint8_t *colBytes, uint8_t rowNum);


