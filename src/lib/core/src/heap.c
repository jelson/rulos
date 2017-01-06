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

#include <stdlib.h>

#include "clock.h"
#include "heap.h"

void heap_init(Heap *heap)
{
	heap->heap_count = 0;
}

void heap_swap(HeapEntry *he, int off0, int off1)
{
	HeapEntry tmp = he[off0];
	he[off0] = he[off1];
	he[off1] = tmp;
}

void heap_bubble(HeapEntry *he, int ptr)
{
	while (ptr > 0)
	{
		int parent = ptr >> 1;
		if (later_than(he[ptr].key, he[parent].key)) { return; }	 // already correct

		heap_swap(he, parent, ptr);
		ptr = parent;
	}
}

void heap_insert(Heap *heap, Time key, ActivationFuncPtr func, void *data)
{
	int hc = heap->heap_count;
	assert(hc < HEAP_CAPACITY);	// heap overflow
	heap->heap[hc].key = key;
	heap->heap[hc].activation.func = func;
	heap->heap[hc].activation.data = data;
	heap_bubble(heap->heap, hc);
	heap->heap_count = hc + 1;

#if 0
	LOGF((logfp, "heap_count %d this act func %08x period %d\n", heap->heap_count, (unsigned) (act->func), key-_last_scheduler_run_us));
#endif

}

int heap_peek(Heap *heap, /*out*/ Time *key, /*out*/ ActivationRecord *act)
{
	int retval = -1;

	if (heap->heap_count == 0)
	{
		goto done;
	}
	*key = heap->heap[0].key;
	*act = heap->heap[0].activation;
	retval = 0;

 done:
	return retval;
}

void heap_pop(Heap *heap)
{
	assert(heap->heap_count > 0);	// heap underflow
	heap->heap[0] = heap->heap[heap->heap_count-1];
	heap->heap_count -= 1;
	int hc = heap->heap_count;
	
	HeapEntry *he = heap->heap;
	/* down-heap */
	int ptr = 0;
	while (1)
	{
		int c0 = ptr*2;
		int c1 = c0 + 1;
		int candidate = ptr;
		if (c0 < hc && later_than(he[candidate].key, he[c0].key))
		{
			candidate = c0;
		}
		if (c1 < hc && later_than(he[candidate].key, he[c1].key))
		{
			candidate = c1;
		}
		if (candidate == ptr)
		{
			goto done;	// down-heaped as far as it goes.
		}
		heap_swap(he, ptr, candidate);
		ptr = candidate;
	}
 done:
	return;
}
