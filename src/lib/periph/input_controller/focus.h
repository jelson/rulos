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

#pragma once

#include "core/rulos.h"
#include "periph/7seg_panel/cursor.h"
#include "periph/7seg_panel/region.h"

struct s_focus_handler;
typedef uint8_t UIEvent;

typedef enum {
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
	const char *label;
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
void focus_register(FocusManager *act, UIEventHandler *handler, RectRegion rr, const char *label);
r_bool focus_is_active(FocusManager *act);


