#include "heap.h"
#include "util.h"

#define HEAP_CAPACITY 16
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
		if (heap[parent].key < heap[ptr].key) { return; }	 // already correct

		heap_swap(parent, ptr);
		ptr = parent;
	}
}

void heap_insert(int key, Activation *act)
{
	assert(heap_count < HEAP_CAPACITY);	// heap overflow
	heap[heap_count].key = key;
	heap[heap_count].activation = act;
	heap_bubble(heap_count);
	heap_count += 1;
}

int heap_peek(/*out*/ int *key, /*out*/ Activation **act)
{
	if (heap_count == 0)
	{
		return -1;
	}
	*key = heap[0].key;
	*act = heap[0].activation;
	return 0;
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
		if (c0 < heap_count && heap[c0].key < heap[candidate].key)
		{
			candidate = c0;
		}
		if (c1 < heap_count && heap[c1].key < heap[candidate].key)
		{
			candidate = c1;
		}
		if (candidate == ptr)
		{
			return;	// down-heaped as far as it goes.
		}
		heap_swap(ptr, candidate);
		ptr = candidate;
	}
}
