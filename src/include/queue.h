#ifndef _queue_h
#define _queue_h

#include <inttypes.h>
#include "util.h"

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

#endif // _queue_h
