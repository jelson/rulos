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

#ifndef _EEPROM_H
#define _EEPROM_H

#include <rocket.h>

// checksum-protected eeprom record
void eeprom_write(uint8_t *buf, int len);

r_bool eeprom_read(uint8_t *buf, int len);
	// returns whether checksum/magic worked. Writes buf either way,
	// so if you get false, better initialize buf yourself.

#endif // _EEPROM_H
