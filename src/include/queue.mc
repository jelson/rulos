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

#define QUEUE_DEFINE(TYPE) \
void TYPE##Queue_init(TYPE##Queue *bq, uint8_t buf_size) \
{ \
	bq->capacity = (buf_size - sizeof(TYPE##Queue)) / sizeof(TYPE); \
	bq->size = 0; \
} \
 \
r_bool TYPE##Queue_append(TYPE##Queue *bq, TYPE elt) \
{ \
	if (bq->size >= bq->capacity) \
	{ \
		return FALSE; \
	} \
	bq->elts[bq->size++] = elt; \
	return TRUE; \
} \
 \
r_bool TYPE##Queue_peek(TYPE##Queue *bq, /*OUT*/ TYPE *elt) \
{ \
	if (bq->size == 0) \
	{ \
		return FALSE; \
	} \
	*elt = bq->elts[0]; \
	return TRUE; \
} \
 \
r_bool TYPE##Queue_pop_n(TYPE##Queue *bq, /*OUT*/ TYPE *elt, uint8_t n) \
{ \
	if (bq->size < n) \
	{ \
		return FALSE; \
	} \
	memcpy(elt, bq->elts, n * sizeof(TYPE)); \
 \
	/* Linear-time pop. Yay! (Yes, I could have written a circular \
	   queue, but then I'd have to test it, and debug it, and you \
	   can see the bind I'm in!) */ \
	memmove(&bq->elts[0], &bq->elts[n], (bq->size-n) * sizeof(TYPE)); \
	bq->size -= n; \
	return TRUE; \
} \
 \
r_bool TYPE##Queue_pop(TYPE##Queue *bq, /*OUT*/ TYPE *elt) \
{ \
	return TYPE##Queue_pop_n(bq, elt, 1); \
} \
 \
 \
uint8_t TYPE##Queue_length(TYPE##Queue *bq) \
{ \
	return bq->size; \
} \
\
void TYPE##Queue_clear(TYPE##Queue *bq) \
{ \
	bq->size = 0; \
} 

