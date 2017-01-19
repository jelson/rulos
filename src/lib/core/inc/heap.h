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

#ifndef __heap_h__
#define __heap_h__

#include <stdint.h>

// had to move this from clock.h to here to resolve circular dependency
typedef int32_t Time;	// in units of usec

typedef void (*ActivationFuncPtr)(void *data);

typedef struct {
	ActivationFuncPtr func;
	void *data;
} ActivationRecord;
	
typedef struct {
	Time key;
	ActivationRecord activation;
} HeapEntry;

#define HEAP_CAPACITY 32
typedef struct {
	HeapEntry heap[HEAP_CAPACITY];
	int heap_count;
} Heap;

void heap_init(Heap *heap);
	// NB: got an old ref to heap_init() (no args) in your main()?
	// Just discard it; it's now handeld by init_clock().
void heap_insert(Heap *heap, Time key, ActivationFuncPtr func, void *data);
int heap_peek(Heap *heap, /*out*/ Time *key, /*out*/ ActivationRecord *act);
	/* rc nonzero => heap empty */
void heap_pop(Heap *heap);

#endif // heap_h