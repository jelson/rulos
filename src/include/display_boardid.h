#include "board_buffer.h"

void boardid_init();

typedef struct {
	ActivationFunc f;
	int stage;
	BoardBuffer b[8];
} BoardActivation_t;

