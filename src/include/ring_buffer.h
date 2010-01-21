#ifndef _RING_BUFFER_H
#define _RING_BUFFER_H

#include "rocket.h"

typedef struct {
	uint8_t capacity;	// actually holds capacity-1 bytes;
						// last byte is a sentinel
	uint8_t insert;
	uint8_t remove;
	uint8_t buf[1];		// dummy byte to "automatically" allocate extra byte
} RingBuffer;

#define DECLARE_RING_BUFFER(name, capacity)	\
	union {	\
		RingBuffer name;	\
		uint8_t _storage_##name[sizeof(RingBuffer)+capacity+1];	\
	};
//	uint8_t[sizeof(RingBuffer)+capacity] storage_##name;
//	RingBuffer *name = (RingBuffer*) storage_##name;

void init_ring_buffer(RingBuffer *rb, uint16_t allocsize);
uint8_t ring_insert_avail(RingBuffer *rb);
void ring_insert(RingBuffer *rb, uint8_t data);
uint8_t ring_remove_avail(RingBuffer *rb);
uint8_t ring_remove(RingBuffer *rb);

#endif // _RING_BUFFER_H
