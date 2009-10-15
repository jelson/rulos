#include "rocket.h"

void ByteQueue_init(ByteQueue *bq, uint8_t buf_size)
{
	bq->capacity = buf_size - sizeof(ByteQueue);
	bq->size = 0;
}

uint8_t ByteQueue_append(ByteQueue *bq, uint8_t elt)
{
	if (bq->size >= bq->capacity)
	{
		return FALSE;
	}
	bq->elts[bq->size++] = elt;
	return TRUE;
}

uint8_t ByteQueue_peek(ByteQueue *bq, /*OUT*/ uint8_t *elt)
{
	if (bq->size == 0)
	{
		return FALSE;
	}
	*elt = bq->elts[0];
	return TRUE;
}

uint8_t ByteQueue_pop_n(ByteQueue *bq, /*OUT*/ uint8_t *elt, uint8_t n)
{
	if (bq->size < n)
	{
		return FALSE;
	}
	memcpy(elt, bq->elts, n * sizeof(*elt));

	// Linear-time pop. Yay! (Yes, I could have written a circular
	// queue, but then I'd have to test it, and debug it, and you
	// can see the bind I'm in!)
	memmove(&bq->elts[0], &bq->elts[n], bq->size-n);
	bq->size -= n;
	return TRUE;
}

uint8_t ByteQueue_pop(ByteQueue *bq, /*OUT*/ uint8_t *elt)
{
	return ByteQueue_pop_n(bq, elt, 1);
}


uint8_t ByteQueue_length(ByteQueue *bq)
{
	return bq->size;
}
