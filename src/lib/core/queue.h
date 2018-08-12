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

#include <inttypes.h>
#include "core/util.h"

typedef char Char;
#include "queue.mh"
QUEUE_DECLARE(Char)

/* usage:
declare storage
	uint8_t byte_queue_storage[sizeof(ByteQueue)+8];

provide a macro that converts storage ptr into correct data type
(no need to "store" converted type in precious RAM!)
#define GetByteQueue(act)	((ByteQueue*)act->byte_queue_storage)

init the raw storage, passing actual number of underlying bytes allocated
	ByteQueue_init(GetByteQueue(act), sizeof(act->byte_queue_storage));
*/
