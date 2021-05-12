/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "util.h"

#define QUEUE_DEFINE(TYPE) \
void TYPE##Queue_init(TYPE##Queue *bq, uint8_t buf_size) \
{ \
	bq->capacity = (buf_size - sizeof(TYPE##Queue)) / sizeof(TYPE); \
	bq->size = 0; \
} \
 \
uint8_t TYPE##Queue_free_space(TYPE##Queue *bq) \
{ \
        return bq->capacity - bq->size; \
} \
\
bool TYPE##Queue_append_n(TYPE##Queue *bq, TYPE *elt, uint8_t n) \
{ \
	if (TYPE##Queue_free_space(bq) < n) \
	{ \
		return FALSE; \
	} \
        memcpy(&bq->elts[bq->size], elt, n * sizeof(TYPE)); \
        bq->size += n; \
	return TRUE; \
} \
 \
bool TYPE##Queue_append(TYPE##Queue *bq, TYPE elt) \
{ \
        return TYPE##Queue_append_n(bq, &elt, 1); \
} \
 \
bool TYPE##Queue_peek(TYPE##Queue *bq, /*OUT*/ TYPE *elt) \
{ \
	if (bq->size == 0) \
	{ \
		return FALSE; \
	} \
	*elt = bq->elts[0]; \
	return TRUE; \
} \
 \
bool TYPE##Queue_pop_n(TYPE##Queue *bq, /*OUT*/ TYPE *elt, uint8_t n) \
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
bool TYPE##Queue_pop(TYPE##Queue *bq, /*OUT*/ TYPE *elt) \
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

