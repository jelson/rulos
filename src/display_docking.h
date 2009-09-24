#ifndef display_docking_h
#define display_docking_h

#include "clock.h"
#include "board_buffer.h"
#include "focus.h"

#define DOCK_HEIGHT 4

struct s_ddockact;

typedef struct {
	UIEventHandlerFunc func;
	struct s_ddockact *act;
} DDockHandler;

typedef struct s_ddockact {
	ActivationFunc func;
	BoardBuffer bbuf[DOCK_HEIGHT];
	DDockHandler handler;
	uint8_t focused;
	int xc, yc, r;
} DDockAct;

void ddock_init(DDockAct *act, uint8_t board0, FocusAct *focus);

#endif // display_docking_h
