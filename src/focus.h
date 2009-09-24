#ifndef focus_h
#define focus_h

#include "clock.h"
#include "cursor.h"
#include "input_controller.h"

struct s_focus_handler;
typedef uint8_t UIEvent;

enum {
	uie_focus = 0x81,
	uie_blur = 0x82,
	uie_select = 'c',
	uie_escape = 'd'
};

typedef enum {
	uied_ignore
	}
UIEventDisposition;

typedef UIEventDisposition (*UIEventHandlerFunc)(
	struct s_focus_handler *handler, UIEvent evt);
typedef struct s_focus_handler {
	UIEventHandlerFunc func;
} UIEventHandler;

struct s_focus_act;

typedef struct {
	InputHandlerFunc func;
	struct s_focus_act *act;
		// TODO if we're desperately starving for RAM, we could compute
		// &FocusAct from &act->inputHandler by subtraction (shudder),
		// saving this pointer.
} FocusInputHandler;

typedef struct {
	uint8_t board;
	UIEventHandler *handler;
} FocusChild;
#define NUM_CHILDREN 8

typedef struct s_focus_act {
	//ActivationFunc func;
	CursorAct cursor;
	FocusInputHandler inputHandler;
	FocusChild children[NUM_CHILDREN];
	uint8_t children_size;
	uint8_t selectedChild;
	uint8_t focusedChild;
} FocusAct;

void focus_init(FocusAct *act);
void focus_register(FocusAct *act, UIEventHandler *handler, uint8_t board);

#endif // focus_h

