#ifndef __input_controller_h__
#define __input_controller_h__

typedef struct {
	ActivationFunc func;
	UIEventHandler *topHandler;
	uint8_t topHandlerActive;
} InputControllerAct;

void input_controller_init(InputControllerAct *act, UIEventHandler *topHandler);

#endif // input_controller_h

