/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#ifndef _control_panel_h
#define _control_panel_h

#include "rocket.h"
#include "remote_keyboard.h"
#include "remote_uie.h"
#include "sequencer.h"
#include "display_docking.h"
#include "pong.h"
#include "disco.h"
#include "hpam.h"
#include "idle.h"
#include "screenblanker.h"

#define CONTROL_PANEL_HEIGHT 4
#define CONTROL_PANEL_NUM_CHILDREN 5
#define CP_NO_CHILD (0xff)

struct s_control_child;

typedef struct s_control_child {
	UIEventHandler *uie_handler;
	char *name;
} ControlChild;

typedef struct {
	UIEventHandler *uie_handler;
	const char *name;
	RemoteKeyboardSend rks;
	RemoteUIE ruie;
} CCRemoteCalc;

typedef struct {
	UIEventHandler *uie_handler;
	const char *name;
	Launch launch;
} CCLaunch;

typedef struct {
	UIEventHandler *uie_handler;
	const char *name;
	DDockAct dock;
} CCDock;

typedef struct {
	UIEventHandler *uie_handler;
	const char *name;
	Pong pong;
} CCPong;

typedef struct {
	UIEventHandler *uie_handler;
	const char *name;
	Disco disco;
} CCDisco;

typedef struct s_direct_injector {
	InputInjectorFunc injector_func;
	struct s_control_panel *cp;
} direct_injector_t;

typedef struct s_control_panel {
	UIEventHandlerFunc handler_func;
	Screen4 s4;
	direct_injector_t direct_injector;
	
	ControlChild *children[CONTROL_PANEL_NUM_CHILDREN];
	uint8_t child_count;
	uint8_t selected_child;
	uint8_t active_child;

	Booster booster;

	// actual children listed here to allocate their storage statically.
	CCRemoteCalc ccrc;
	CCLaunch ccl;
	CCDock ccdock;
	CCPong ccpong;
	CCDisco ccdisco;

	IdleAct *idle;
} ControlPanel;

void init_control_panel(ControlPanel *cp, uint8_t board0, uint8_t aux_board0, Network *network, HPAM *hpam, AudioClient *audioClient, IdleAct *idle, ScreenBlanker *screenblanker, JoystickState_t *joystick);

#endif // _control_panel_h
