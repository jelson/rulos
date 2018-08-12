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

#ifndef __USI_TWI_SLAVE_H__
#define __USI_TWI_SLAVE_H__

#include "core/media.h"

typedef uint8_t (*usi_slave_send_func)();

void usi_twi_slave_init(char address, MediaRecvSlot* recv_slot, usi_slave_send_func send_func);

#endif //  __USI_TWI_SLAVE_H__
