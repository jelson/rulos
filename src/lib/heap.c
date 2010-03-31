#include "rocket.h"

#define HEAP_CAPACITY 32
HeapEntry heap[HEAP_CAPACITY];
int heap_count;

void heap_init()
{
	heap_count = 0;
}

void heap_swap(int off0, int off1)
{
	HeapEntry tmp = heap[off0];
	heap[off0] = heap[off1];
	heap[off1] = tmp;
}

void heap_bubble(int ptr)
{
	while (ptr > 0)
	{
		int parent = ptr >> 1;
		if (later_than(heap[ptr].key, heap[parent].key)) { return; }	 // already correct

		heap_swap(parent, ptr);
		ptr = parent;
	}
}

void heap_insert(Time key, Activation *act)
{
	assert(heap_count < HEAP_CAPACITY);	// heap overflow
	heap[heap_count].key = key;
	heap[heap_count].activation = act;
	heap_bubble(heap_count);
	heap_count += 1;
	LOGF((logfp, "heap_count %d this act func %08x period %d\n", heap_count, (unsigned) (act->func), key-_last_scheduler_run_us));
}

int heap_peek(/*out*/ Time *key, /*out*/ Activation **act)
{
	int retval = -1;

	if (heap_count == 0)
	{
		goto done;
	}
	*key = heap[0].key;
	*act = heap[0].activation;
	retval = 0;

 done:
	return retval;
}

void heap_pop()
{
	assert(heap_count > 0);	// heap underflow
	heap[0] = heap[heap_count-1];
	heap_count -= 1;
	
	/* down-heap */
	int ptr = 0;
	while (1)
	{
		int c0 = ptr*2;
		int c1 = c0 + 1;
		int candidate = ptr;
		if (c0 < heap_count && later_than(heap[candidate].key, heap[c0].key))
		{
			candidate = c0;
		}
		if (c1 < heap_count && later_than(heap[candidate].key, heap[c1].key))
		{
			candidate = c1;
		}
		if (candidate == ptr)
		{
			goto done;	// down-heaped as far as it goes.
		}
		heap_swap(ptr, candidate);
		ptr = candidate;
	}
 done:
	return;
}
