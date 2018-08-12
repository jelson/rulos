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

#include "rulos.h"
#include "rocket_ring_buffer.h"

void init_ring_buffer(RingBuffer *rb, uint16_t allocsize)
{
	rb->capacity = allocsize - sizeof(RingBuffer);
	rb->insert = 0;
	rb->remove = 0;
}

// invariant: rb->buf[rb->insert] is an empty cell.
// (hence the capacity-1 property.)

uint8_t ring_insert_avail(RingBuffer *rb)
{
	return rb->capacity - 1 - ring_remove_avail(rb);
}

void ring_insert(RingBuffer *rb, uint8_t data)
{
	rb->buf[rb->insert] = data;
	rb->insert += 1;
	if (rb->insert >= rb->capacity)
	{
		rb->insert = 0;
	}
	assert(rb->insert != rb->remove);	// invariant fail: buffer full
}

uint8_t ring_remove_avail(RingBuffer *rb)
{
	int16_t result = rb->insert - rb->remove;
	if (result<0)
	{
		result += rb->capacity;
	}
	return result;
}

uint8_t ring_remove(RingBuffer *rb)
{
	assert(rb->remove != rb->insert);	// buffer empty
	uint8_t result = rb->buf[rb->remove];
	rb->remove += 1;
	if (rb->remove >= rb->capacity)
	{
		rb->remove = 0;
	}
	return result;
}

