#ifndef __focus_h__
#define __focus_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif

struct s_focus_handler;
typedef uint8_t UIEvent;

enum {
	uie_focus = 0x81,
	uie_select = 'c',
	uie_escape = 'd',
	uie_right = 'a',
	uie_left = 'b',
	evt_notify = 0x80,
	evt_remote_escape = 0x82,
	evt_idle_nowidle = 0x83,
	evt_idle_nowactive = 0x84,
} EventName;

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
	char *label;
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
void focus_register(FocusManager *act, UIEventHandler *handler, RectRegion rr, char *label);
r_bool focus_is_active(FocusManager *act);

#endif // focus_h

