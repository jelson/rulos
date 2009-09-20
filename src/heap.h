#ifndef heap_h
#define heap_h

struct s_activation;
typedef void (*ActivationFunc)(struct s_activation *act);
typedef struct s_activation {
	ActivationFunc func;
} Activation;

typedef struct {
	int key;
	Activation *activation;
} HeapEntry;

void heap_init();
void heap_insert(int key, Activation *act);
int heap_peek(/*out*/ int *key, /*out*/ Activation **act);
	/* rc nonzero => heap empty */
void heap_pop();

#endif // heap_h
