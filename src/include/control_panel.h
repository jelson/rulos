#ifndef _control_panel_h
#define _control_panel_h

#include "rocket.h"
#include "remote_keyboard.h"
#include "remote_uie.h"
#include "sequencer.h"
#include "display_docking.h"
#include "pong.h"

#define CONTROL_PANEL_HEIGHT 4
#define CONTROL_PANEL_NUM_CHILDREN 4
#define CP_NO_CHILD (0xff)

struct s_control_child;

typedef struct s_control_child {
	UIEventHandler *uie_handler;
	char *name;
} ControlChild;

typedef struct {
	UIEventHandler *uie_handler;
	char *name;
	RemoteKeyboardSend rks;
	RemoteUIE ruie;
} CCRemoteCalc;

typedef struct {
	UIEventHandler *uie_handler;
	char *name;
	Launch launch;
} CCLaunch;

typedef struct {
	UIEventHandler *uie_handler;
	char *name;
	DDockAct dock;
} CCDock;

typedef struct {
	UIEventHandler *uie_handler;
	char *name;
	Pong pong;
} CCPong;

typedef struct s_control_panel {
	UIEventHandlerFunc handler_func;
	BoardBuffer bbuf[CONTROL_PANEL_HEIGHT];
	BoardBuffer *btable[CONTROL_PANEL_HEIGHT];
	RectRegion rrect;

	struct s_direct_injector {
		InputInjectorFunc injector_func;
		struct s_control_panel *cp;
	} direct_injector;
	
	ControlChild *children[CONTROL_PANEL_NUM_CHILDREN];
	uint8_t child_count;
	uint8_t selected_child;
	uint8_t active_child;

	// actual children listed here to allocate their storage statically.
	CCRemoteCalc ccrc;
	CCLaunch ccl;
	CCDock ccdock;
	CCPong ccpong;
} ControlPanel;

void init_control_panel(ControlPanel *cp, uint8_t board0, Network *network);

#endif // _control_panel_h
