#ifndef input_controller_h
#define input_controller_h

#include "clock.h"
#include "queue.h"

struct s_inputHandler;
typedef void (*InputHandlerFunc)(struct s_inputHandler *handler, char key);
typedef struct s_inputHandler {
	InputHandlerFunc func;
} InputHandler;

typedef struct {
	ActivationFunc func;
	InputHandler *topHandler;
} InputControllerAct;

void input_controller_init(InputControllerAct *act, InputHandler *topHandler);

#endif // input_controller_h

