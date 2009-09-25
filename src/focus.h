#ifndef focus_h
#define focus_h

#include "clock.h"
#include "cursor.h"

struct s_focus_handler;
typedef uint8_t UIEvent;

enum {
	uie_focus = 0x81,
	uie_select = 'c',
	uie_escape = 'd',
	uie_right = 'a',
	uie_left = 'b',
};

typedef enum {
	uied_accepted,	// child consumed event
	uied_blur		// child releases focus
	}
UIEventDisposition;

typedef UIEventDisposition (*UIEventHandlerFunc)(
	struct s_focus_handler *handler, UIEvent evt);
typedef struct s_focus_handler {
	UIEventHandlerFunc func;
} UIEventHandler;

struct s_focus_act;

typedef struct {
	RectRegion rr;
	UIEventHandler *handler;
} FocusChild;
#define NUM_CHILDREN 8

typedef struct s_focus_act {
	UIEventHandlerFunc func;
	CursorAct cursor;
	FocusChild children[NUM_CHILDREN];
	uint8_t children_size;
	uint8_t selectedChild;
	uint8_t focusedChild;
} FocusManager;

void focus_init(FocusManager *act);
void focus_register(FocusManager *act, UIEventHandler *handler, RectRegion rr);

#endif // focus_h

