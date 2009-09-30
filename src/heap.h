#ifndef heap_h
#define heap_h

#include <inttypes.h>

struct s_activation;
typedef void (*ActivationFunc)(struct s_activation *act);
typedef struct s_activation {
	ActivationFunc func;
} Activation;

typedef struct {
	uint32_t key;
	Activation *activation;
} HeapEntry;

void heap_init();
void heap_insert(uint32_t key, Activation *act);
int heap_peek(/*out*/ uint32_t *key, /*out*/ Activation **act);
	/* rc nonzero => heap empty */
void heap_pop();

#endif // heap_h
