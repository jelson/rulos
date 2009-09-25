#ifndef input_controller_h
#define input_controller_h

#include "clock.h"
#include "queue.h"
#include "focus.h"

typedef struct {
	ActivationFunc func;
	UIEventHandler *topHandler;
	uint8_t topHandlerActive;
} InputControllerAct;

void input_controller_init(InputControllerAct *act, UIEventHandler *topHandler);

#endif // input_controller_h

