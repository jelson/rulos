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

#include "rocket.h"
#include "queue.h"
#include "ring_buffer.h"

QUEUE_DECLARE(short)

#include "queue.mc"
QUEUE_DEFINE(short)

#define NUM_ELTS 4

void test_shortqueue()
{
	uint8_t space[sizeof(shortQueue)+sizeof(short)*NUM_ELTS];
	shortQueue *sq = (shortQueue*) space;
	shortQueue_init(sq, sizeof(space));

	short in = 0, out = 0;

	while (TRUE)
	{
		if (deadbeef_rand() & 1)
		{
			r_bool rc = shortQueue_append(sq, in);
			if (rc)
			{
				in++;
				LOGF((logfp, "in: %d\n", in));
			}
			else
			{
				LOGF((logfp, "full\n"));
			}
		}
		else
		{
			short val;
			r_bool rc = shortQueue_pop(sq, &val);
			if (rc) {
				assert(val==out);
				LOGF((logfp, "        out: %d\n", out));
				out++;
			}
			else
			{
				LOGF((logfp, "empty\n"));
			}
		}
	}
}

void test_ring_buffer()
{
	uint8_t _storage_rb[sizeof(RingBuffer)+17+1];
	RingBuffer *rb = (RingBuffer*) _storage_rb;
	init_ring_buffer(rb, sizeof(_storage_rb));

	uint8_t in = 0, out = 0;

	while (TRUE)
	{
		if (deadbeef_rand() & 1)
		{
			if (ring_insert_avail(rb)>0)
			{
				ring_insert(rb, in);
				LOGF((logfp, "in: %d\n", in));
				in++;
			}
			else
			{
				LOGF((logfp, "full\n"));
			}
		}
		else
		{
			uint8_t ra = ring_remove_avail(rb);
			LOGF((logfp, "ra %d\n", ra));
			if (ra)
			{
				uint8_t val;
				val = ring_remove(rb);
				assert(val==out);
				LOGF((logfp, "        out: %d\n", out));
				out++;
			}
			else
			{
				LOGF((logfp, "empty\n"));
			}
		}
	}
}

int main()
{
	//test_shortqueue();
	test_ring_buffer();
	return 0;
}
