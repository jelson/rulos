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
