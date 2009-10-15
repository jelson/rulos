#ifndef _queue_h
#define _queue_h

#include <inttypes.h>
#include "util.h"

typedef struct {
	uint8_t capacity;
	uint8_t size;
	uint8_t elts[0];
} ByteQueue;	// TODO worry about variable length elts later

void ByteQueue_init(ByteQueue *bq, uint8_t buf_size);
uint8_t ByteQueue_append(ByteQueue *bq, uint8_t elt);
uint8_t ByteQueue_peek(ByteQueue *bq, /*OUT*/ uint8_t *elt);
uint8_t ByteQueue_pop(ByteQueue *bq, /*OUT*/ uint8_t *elt);
uint8_t ByteQueue_pop_n(ByteQueue *bq, /*OUT*/ uint8_t *elt, uint8_t n);
uint8_t ByteQueue_length(ByteQueue *bq);

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
